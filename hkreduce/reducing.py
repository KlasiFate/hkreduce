import multiprocessing
from enum import Enum, auto
from multiprocessing.synchronize import BoundedSemaphore
from typing import Any, cast

import numpy as np
from cantera import Solution
from numpy.typing import NDArray

from .algorithms import create_matrix_for_drg, create_matrix_for_drgep, create_matrix_for_pfa
from .config import Config
from .cpp_interface import CSRAdjacencyMatrix
from .logging import get_logger
from .typing import ReducingMethod
from .utils import NumpyArrayDumper, Worker, WorkersManager, create_unique_file, get_species_indexes, load_model


class Command(int, Enum):
    REDUCE = auto()
    STOP = auto()


class Answer(int, Enum):
    MATRIX_CREATED = auto()
    ERROR = auto()
    REDUCED = auto()


class Reducer(Worker):
    def __init__(
        self,
        config: Config,
        state_saver: NumpyArrayDumper,
        ai_condition_idx: int,
        state_idx: int,
        sem: BoundedSemaphore,
    ) -> None:
        super().__init__(name=f"reduces_for_{ai_condition_idx}_ai_condition_and_{state_idx}_state")

        self.config = config
        self.state_saver = state_saver
        self.ai_condition_idx = ai_condition_idx
        self.state_idx = state_idx
        self.sem = sem

        self.logger = get_logger()

    def _create_matrix(self, model: Solution) -> CSRAdjacencyMatrix:
        with self.state_saver.open("r"):
            state = self.state_saver.read_data()
        temperature, pressure = state[:2:]
        mass_fractions = state[2::]

        if self.config.reducing_task_config.method == ReducingMethod.DRG:
            return create_matrix_for_drg(model, temperature, pressure, mass_fractions)
        if self.config.reducing_task_config.method == ReducingMethod.DRGEP:
            return create_matrix_for_drgep(model, temperature, pressure, mass_fractions)
        return create_matrix_for_pfa(model, temperature, pressure, mass_fractions)

    def _reduce(self, sources: NDArray[np.uintp], threshold: float, matrix: CSRAdjacencyMatrix) -> NumpyArrayDumper:
        retained_species = matrix.run_reducing(self.config.reducing_task_config.method.name, threshold, sources)  # type: ignore[arg-type]
        prefix = "retained_species_for_{}_and_{}_state_case_with_threshold_{}_".format(
            self.ai_condition_idx, self.state_idx, str(threshold).replace(".", "__")
        )
        filepath = create_unique_file(
            dir=self.tmp_dir,
            prefix=prefix,
            suffix=".npy",
        )
        with NumpyArrayDumper(self.tmp_dir, filepath.name).open("w") as retained_species_saver:
            retained_species_saver.write_data(retained_species)
        return retained_species_saver

    def _target(self) -> None:
        try:
            with self.sem:
                model = load_model(self.config.reducing_task_config.model)
                self.logger.info(
                    "Create matrix for {state_idx} state of {ai_condition_idx} case",
                    state_idx=self.state_idx,
                    ai_condition_idx=self.ai_condition_idx,
                )
                matrix = self._create_matrix(model)

                self._send_msg_to_parent((Answer.MATRIX_CREATED, ()))

                sources = get_species_indexes(self.config.reducing_task_config.target_species, model)

            while True:
                command, command_args = cast(tuple[Command, tuple[Any, ...]], self._get_msg_from_parent())
                if command == Command.STOP:
                    return
                threshold = cast(tuple[float], command_args)[0]
                with self.sem:
                    retained_species_saver = self._reduce(sources, threshold, matrix)
                    self._send_msg_to_parent((Answer.REDUCED, (retained_species_saver,)))
        except KeyboardInterrupt:
            self.logger.info(
                "Cancelling reducer worker for {state_idx} state of {ai_condition_idx} case",
                state_idx=self.state_idx,
                ai_condition_idx=self.ai_condition_idx,
            )
        except BaseException as error:
            self.logger.opt(exception=error).critical(
                "Error while reducing or creating matrix for {state_idx} state of {ai_condition_idx} case",
                state_idx=self.state_idx,
                ai_condition_idx=self.ai_condition_idx,
            )
            self._send_msg_to_parent((Answer.ERROR, ()))
            if not isinstance(error, Exception):
                raise
        finally:
            self.logger.complete()


class ReducersManager(WorkersManager):
    def __init__(
        self,
        config: Config,
        samples_savers: list[NumpyArrayDumper],
    ) -> None:
        self.config = config
        self.samples_savers = samples_savers

        self._sem = multiprocessing.BoundedSemaphore(config.num_threads)

        super().__init__(self._create_reducers())

        self._matrixes_created = False

        model = load_model(self.model_path)
        self.retained_species = set(get_species_indexes(config.reducing_task_config.retained_species, model=model))

    def _create_state_saver(self, ai_cond_idx: int, state_idx: int, state: NDArray[np.float64]) -> NumpyArrayDumper:
        state_saver = NumpyArrayDumper(
            dir=self.tmp_dir,
            filename=create_unique_file(
                dir=self.tmp_dir,
                prefix=f"state_for_{ai_cond_idx}_ai_condition_and_{state_idx}_state_",
                suffix=".npy",
            ).name,
        )
        with state_saver.open("w"):
            state_saver.write_data(state)
        return state_saver

    def _create_reducers(self) -> list[Reducer]:
        reducers: list[Reducer] = []
        for ai_cond_idx, sample_saver in enumerate(self.samples_savers):
            with sample_saver.open("r"):
                for state_idx in range(sample_saver.saved_arrays_count):
                    state_saver = self._create_state_saver(ai_cond_idx, state_idx, sample_saver.read_data())

                    reducers.append(
                        Reducer(
                            config=self.config,
                            state_saver=state_saver,
                            ai_condition_idx=ai_cond_idx,
                            state_idx=state_idx,
                            sem=self._sem,
                        )
                    )

        return reducers

    def open(self) -> None:
        self.logger.info("Creating matrixes")
        super().open()

    def __enter__(self) -> "ReducersManager":
        super().__enter__()

    def reduce(self, threshold: float) -> set[int]:
        if not self._matrixes_created:
            for reducer in self.workers:
                message, details = cast(tuple[Answer, tuple[Any, ...]], reducer.get_msg_from_worker())
                if message != Answer.MATRIX_CREATED:
                    raise RuntimeError("Error in reducer process. Creating matrix failed")

            self.logger.info("Matrixes created")

            self._matrixes_created = True

        for reducer in self.workers:
            reducer.send_to_worker((Command.REDUCE, (threshold,)))

        retained_species = self.retained_species
        for reducer in self.workers:
            message, details = cast(tuple[Answer, tuple[Any, ...]], reducer.get_msg_from_worker())

            if message != Answer.REDUCED:
                raise RuntimeError("Error in reducer process. Reduce failed")

            retained_species_saver = cast(NumpyArrayDumper, details[0])
            with retained_species_saver.open("r"):
                retained_species = retained_species.union(cast(NDArray[np.uintp], retained_species_saver.read_data()))

        return retained_species
