import multiprocessing
import shutil
from enum import Enum, auto
from multiprocessing.connection import Connection
from multiprocessing.context import Process
from multiprocessing.synchronize import BoundedSemaphore
from typing import Any, cast

import numpy as np
from cantera import Solution  # type: ignore[import-untyped]
from loguru import logger
from numpy.typing import NDArray

from .algorithms import create_matrix_for_drg, create_matrix_for_drgep, create_matrix_for_pfa
from .config import ReducingTaskConfig
from .cpp_interface import CSRAdjacencyMatrix
from .cpp_interface import run as run_reducing
from .errors import AutoretrievingInitialThresholdError, ReducingError, SimulationError
from .logging import setup_config
from .simulation import SimulationManager
from .typing import PathLike, ReducingMethod
from .utils import NumpyArrayDumper, WorkersCloser, create_unique_file


class Command(int, Enum):
    REDUCE = auto()
    STOP = auto()


class Answer(int, Enum):
    INITIALIZED = auto()
    ERROR = auto()
    REDUCED = auto()


class Reducer:
    def __init__(
        self,
        model_path: PathLike,
        method: ReducingMethod,
        state_saver: NumpyArrayDumper,
        sources_saver: NumpyArrayDumper,
        tmp_dir: PathLike,
        ai_condition_idx: int,
        conn: Connection,
        sem: BoundedSemaphore,
        *,
        debug: bool,
    ) -> None:
        self.model_path = model_path
        self.method = method
        self.state_saver = state_saver
        self.sources_saver = sources_saver

        self.tmp_dir = tmp_dir
        self.ai_condition_idx = ai_condition_idx
        self.conn = conn
        self.sem = sem
        self.debug = debug

    def _create_matrix(self) -> CSRAdjacencyMatrix:
        model = Solution(self.model_path)

        with self.state_saver.open("r"):
            state = self.state_saver.read_data()
        temperature, pressure = state[::2]
        mass_fractions = state[2::]

        if self.method == "DRG":
            return create_matrix_for_drg(model, temperature, pressure, mass_fractions)
        if self.method == "DRGEP":
            return create_matrix_for_drgep(model, temperature, pressure, mass_fractions)
        return create_matrix_for_pfa(model, temperature, pressure, mass_fractions)

    def _reduce(self, threshold: float, matrix: CSRAdjacencyMatrix) -> NumpyArrayDumper:
        with self.sources_saver.open("r"):
            sources = cast(NDArray[np.uintp], self.sources_saver.read_data())
        retained_species = run_reducing(matrix, self.method, threshold, sources)
        prefix = f"retained_species_for_{self.ai_condition_idx}_case_with_threshold_{str(threshold).replace('.', '__')}"
        filepath = create_unique_file(
            dir=self.tmp_dir,
            prefix=prefix,
            suffix=".npy",
        )
        with NumpyArrayDumper(self.tmp_dir, filepath.name).open("w") as retained_species_saver:
            retained_species_saver.write_data(retained_species)
        return retained_species_saver

    def run(self) -> None:
        setup_config(debug=self.debug)

        try:
            with self.sem:
                matrix = self._create_matrix()
            self.conn.send((Answer.INITIALIZED, ()))
        except Exception:
            logger.opt(exception=True).critical("Error while creating csr matrix")
            self.conn.send((Answer.ERROR, ()))
            return

        try:
            while True:
                command, command_args = cast(tuple[Command, tuple[Any, ...]], self.conn.recv())
                if command == Command.STOP:
                    return
                threshold = cast(tuple[float], command_args)[0]
                with self.sem:
                    retained_species_saver = self._reduce(threshold, matrix)
                    self.conn.send((Answer.REDUCED, (retained_species_saver)))
        except Exception:
            logger.opt(exception=True).critical("Error while reducing")
            self.conn.send((Answer.ERROR, ()))
            return


class ReducersManager:
    def __init__(
        self,
        model: Solution,
        reducing_task_config: ReducingTaskConfig,
        num_threads: int,
        tmp_dir: PathLike,
        output_model_path: PathLike,
        *,
        debug: bool,
    ) -> None:
        self.model = model
        self.reducing_task_config = reducing_task_config
        self.num_threads = num_threads
        self.tmp_dir = tmp_dir
        self.output_model_path = output_model_path
        self.debug = debug

        self._sources_saver = self._create_sources_saver()
        self._sem = multiprocessing.BoundedSemaphore(num_threads)

    def _create_state_saver(self, ai_cond_idx: int, state_idx: int, state: NDArray[np.float64]) -> NumpyArrayDumper:
        state_saver = NumpyArrayDumper(
            dir=self.tmp_dir,
            filename=create_unique_file(
                dir=self.tmp_dir,
                prefix=f"state_for_{ai_cond_idx}_ai_condition_and_{state_idx}_state",
                suffix=".npy",
            ).name,
        )
        with state_saver.open("w"):
            state_saver.write_data(state)
        return state_saver

    def _create_sources_saver(self) -> NumpyArrayDumper:
        sources = np.empty((len(self.reducing_task_config.target_species)), dtype=np.uintp)
        for i, source in enumerate(self.reducing_task_config.target_species):
            sources[i] = self.model.species_index(source)
        sources_saver = NumpyArrayDumper(
            dir=self.tmp_dir,
            filename=create_unique_file(
                dir=self.tmp_dir,
                prefix="sources",
                suffix=".npy",
            ).name,
        )
        with sources_saver.open("w"):
            sources_saver.write_data(sources)
        return sources_saver

    def _create_reducers(self, samples_savers: list[NumpyArrayDumper]) -> list[tuple[Process, Connection]]:
        reducers: list[tuple[Process, Connection]] = []
        for ai_cond_idx, sample_saver in enumerate(samples_savers):
            with sample_saver.open("r"):
                for state_idx in range(sample_saver.saved_arrays_count):
                    state_saver = self._create_state_saver(ai_cond_idx, state_idx, sample_saver.read_data())
                    for_manager, for_reducer = multiprocessing.Pipe(duplex=True)

                    reducer = Reducer(
                        model_path=self.reducing_task_config.model,
                        method=self.reducing_task_config.method,
                        state_saver=state_saver,
                        sources_saver=self._sources_saver,
                        tmp_dir=self.tmp_dir,
                        sem=self._sem,
                        ai_condition_idx=ai_cond_idx,
                        conn=for_reducer,
                        debug=self.debug,
                    )

                    process_name = f"reduces_for_{ai_cond_idx}_ai_condition_and_{state_idx}_state"
                    process = multiprocessing.Process(target=reducer.run, name=process_name, daemon=True)
                    reducers.append((process, for_manager))

        return reducers

    def _reduce(self, reducers: list[tuple[Process, Connection]], threshold: float) -> set[int]:
        for process, conn in reducers:
            if not process.is_alive():
                raise RuntimeError("Reducer process is dead")

            conn.send((Command.REDUCE, ()))

            message, details = cast(tuple[Answer, tuple[Any, ...]], conn.recv())

            if message != Answer.REDUCED:
                raise RuntimeError("Error in reducer process. Reduce failed")

            retained_species: set[int] = set()
            retained_species_saver = cast(NumpyArrayDumper, details[0])
            with retained_species_saver.open("w"):
                retained_species.union(cast(NDArray[np.uintp], retained_species_saver.read_data()))

        return retained_species

    def _retrieve_initial_threshold(
        self, reducers: list[tuple[Process, Connection]], original_ignition_delays: list[float]
    ) -> tuple[float, PathLike, set[int]]:
        for i in range(self.reducing_task_config.initial_threshold_auto_retrieving_attempts):
            threshold = self.reducing_task_config.initial_threshold * (
                self.reducing_task_config.initial_threshold_auto_retrieving_multiplier**i
            )
            logger.debug(f"Attempt {threshold} as initial threshold")

            logger.info(f"Reduce model with threshold: {threshold}")
            retained_species = self._reduce(reducers, threshold)
            logger.info(f"Model reduced with {len(retained_species)} species and threshold {threshold}")

            __, model_path = self._create_reduced_model(retained_species=retained_species)
            logger.info(f"Created reduced model with {len(retained_species)} species: `{model_path}`")

            error = self._calc_error(model_path, original_ignition_delays)
            if error < self.reducing_task_config.max_error:
                return threshold, model_path, retained_species

        raise AutoretrievingInitialThresholdError("Auto retrieving initial threshold failed because no threshold")

    def _create_reduced_model(self, retained_species: set[int]) -> tuple[Solution, PathLike]:
        removed_species = set(range(self.model.n_species))
        removed_species.difference(retained_species)
        model_species = self.model.species
        removed_species_names = {model_species[removed_specy] for removed_specy in removed_species}

        # TODO: read reactions from file rather than model. Because it is not known reaction modifying has effect or not

        reactions_of_reduced_model: list[Any] = []
        for reaction in self.model.reactions():
            if any(product in removed_species_names for product in reaction.products):
                continue

            if any(reactant in removed_species_names for reactant in reaction.reactants):
                continue

            third_body = reaction.third_body
            if third_body:
                retained_efficiencies: dict[str, float] = {}
                for specy_name, efficiency in cast(dict[str, float], third_body.efficiencies).items():
                    if specy_name not in removed_species_names and efficiency != 0.0:
                        retained_efficiencies[specy_name] = efficiency  # noqa: PERF403

                if len(retained_efficiencies) == 0:
                    continue

                third_body.efficiencies = retained_efficiencies

            reactions_of_reduced_model.append(reaction)

        species_of_reduced_model: list[str] = [specy.name for specy in self.model.species]

        model = Solution(
            species=species_of_reduced_model,
            reactions=reactions_of_reduced_model,
            thermo="IdealGas",
            kinetics="GasKinetics",
        )
        model_filename = create_unique_file(
            dir=self.tmp_dir, prefix=f"reduced_model_with_{len(species_of_reduced_model)}_species", suffix=".yaml"
        )
        model.write_yaml(filename=model_filename)

        return model, model_filename

    def _calc_error(self, model_path: PathLike, original_ignition_delays: list[float]) -> float:
        try:
            __, ignition_delays = SimulationManager(
                model_path=model_path,
                reducing_task_config=self.reducing_task_config,
                num_threads=self.num_threads,
                tmp_dir=self.tmp_dir,
                debug=self.debug,
                only_ignition_delays=True,
            ).run()
        except SimulationError:
            return 1

        max_error: float = 0.0
        for i in range(len(original_ignition_delays)):
            error = min(abs(1 - ignition_delays[i] / original_ignition_delays[i]), 1)
            if max_error < error:
                max_error = error
        return max_error

    def run(self) -> None:
        logger.info("Start simulation for retrieve samples")
        samples_savers, original_ignition_delays = SimulationManager(
            model_path=self.reducing_task_config.model,
            reducing_task_config=self.reducing_task_config,
            num_threads=self.num_threads,
            tmp_dir=self.tmp_dir,
            debug=self.debug,
            only_ignition_delays=False,
        ).run()
        logger.info("Samples is got")

        logger.debug("Creating reducers")
        reducers = self._create_reducers(samples_savers)

        logger.debug("Start reducers")
        with WorkersCloser(reducers):
            initial_threshold = self.reducing_task_config.initial_threshold

            if not self.reducing_task_config._initial_threshold_set_by_user:  # noqa: SLF001
                logger.info("Start auto retrieving initial threshold")
                initial_threshold, model_path, retained_species = self._retrieve_initial_threshold(
                    reducers, original_ignition_delays
                )
                logger.info(f"Use {initial_threshold} as initial threshold")
            else:
                logger.info(f"Reduce model with threshold: {initial_threshold}")
                retained_species = self._reduce(reducers, initial_threshold)
                logger.info(f"Model reduced with {len(retained_species)} species and threshold {initial_threshold}")

                __, model_path = self._create_reduced_model(retained_species)
                logger.info(f"Created reduced model with {len(retained_species)} species: `{model_path}`")

                error = self._calc_error(model_path, original_ignition_delays)
                if error > self.reducing_task_config.max_error:
                    raise ReducingError("Invalid user initial threshold")

            threshold = initial_threshold
            while True:
                if self.reducing_task_config.threshold_increment:
                    threshold += self.reducing_task_config.threshold_increment
                else:
                    threshold += initial_threshold

                logger.info(f"Reduce model with threshold: {initial_threshold}")
                retained_species = self._reduce(reducers, initial_threshold)
                logger.info(f"Model reduced with {len(retained_species)} species and threshold {initial_threshold}")

                __, model_path = self._create_reduced_model(retained_species)
                logger.info(f"Created reduced model with {len(retained_species)} species: `{model_path}`")

                error = self._calc_error(model_path, original_ignition_delays)
                if error > self.reducing_task_config.max_error:
                    logger.info(
                        f"Reduced model with {len(retained_species)} species at `{model_path}` give too large error"
                    )
                    break

            logger.debug("Stop reducers")
            for __, conn in reducers:
                conn.send((Command.STOP, ()))

            logger.info(f"Use model with {len(retained_species)} species at `{model_path}` as result")
            try:
                shutil.copyfile(model_path, self.output_model_path)
            except Exception as error:
                raise RuntimeError(f"Failed to copy result reduced model to {self.output_model_path}") from error
