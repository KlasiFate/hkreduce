import multiprocessing
from enum import Enum, auto
from multiprocessing.connection import Connection
from multiprocessing.context import Process
from multiprocessing.synchronize import BoundedSemaphore
from typing import Any, cast

import numpy as np
from cantera import Solution  # type: ignore[import-untyped]
from numpy.typing import NDArray

from .algorithms import create_matrix_for_drg, create_matrix_for_drgep, create_matrix_for_pfa
from .cpp_interface import CSRAdjacencyMatrix
from .logging import get_logger
from .typing import PathLike, ReducingMethod
from .utils import NumpyArrayDumper, WorkersCloser, create_unique_file


class Command(int, Enum):
    REDUCE = auto()
    STOP = auto()


class Answer(int, Enum):
    INITIALIZED = auto()
    MATRIX_CREATED = auto()
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
    ) -> None:
        self.model_path = model_path
        self.method = method
        self.state_saver = state_saver
        self.sources_saver = sources_saver

        self.tmp_dir = tmp_dir
        self.ai_condition_idx = ai_condition_idx
        self.conn = conn
        self.sem = sem

        self.logger = get_logger()

    def _create_matrix(self) -> CSRAdjacencyMatrix:
        model = Solution(self.model_path)

        with self.state_saver.open("r"):
            state = self.state_saver.read_data()
        temperature, pressure = state[:2:]
        mass_fractions = state[2::]

        if self.method == ReducingMethod.DRG:
            return create_matrix_for_drg(model, temperature, pressure, mass_fractions)
        if self.method == ReducingMethod.DRGEP:
            return create_matrix_for_drgep(model, temperature, pressure, mass_fractions)
        return create_matrix_for_pfa(model, temperature, pressure, mass_fractions)

    def _reduce(self, threshold: float, matrix: CSRAdjacencyMatrix) -> NumpyArrayDumper:
        with self.sources_saver.open("r"):
            sources = cast(NDArray[np.uintp], self.sources_saver.read_data())
        retained_species = matrix.run_reducing(self.method.name, threshold, sources)  # type: ignore[arg-type]
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
        self.conn.send((Answer.INITIALIZED, ()))

        try:
            self.logger.info(f"Creating matrix for one of states of {self.ai_condition_idx} case")
            with self.sem:
                matrix = self._create_matrix()
                self.conn.send((Answer.MATRIX_CREATED, ()))
            self.logger.info(f"Matrix created for one of states of {self.ai_condition_idx} case")
            while True:
                command, command_args = cast(tuple[Command, tuple[Any, ...]], self.conn.recv())
                if command == Command.STOP:
                    return
                threshold = cast(tuple[float], command_args)[0]
                with self.sem:
                    retained_species_saver = self._reduce(threshold, matrix)
                    self.conn.send((Answer.REDUCED, (retained_species_saver,)))
        except KeyboardInterrupt:
            self.logger.info(f"Cancelling reducer process for one of states of {self.ai_condition_idx} case")
        except Exception:  # noqa: BLE001
            self.logger.opt(exception=True).critical(
                f"Error while reducing or creating matrix  for one of states of {self.ai_condition_idx} case"
            )
            self.conn.send((Answer.ERROR, ()))
        finally:
            self.logger.complete()


class ReducersManager:
    def __init__(
        self,
        model_path: PathLike,
        method: ReducingMethod,
        samples_savers: list[NumpyArrayDumper],
        sources: NDArray[np.uintp],
        retained_species: NDArray[np.uintp],
        tmp_dir: PathLike,
        num_threads: int,
    ) -> None:
        self.model_path = model_path
        self.method = method
        self.samples_savers = samples_savers
        self.retained_species = retained_species

        self.tmp_dir = tmp_dir

        self._sources_saver = self._create_sources_saver(sources, tmp_dir)

        self._reducers: list[tuple[Process, Connection]] | None = None
        self._workers_closer: WorkersCloser | None = None
        self._sem = multiprocessing.BoundedSemaphore(num_threads)

        self._matrixes_created = False

    @classmethod
    def _create_sources_saver(cls, sources: NDArray[np.uintp], tmp_dir: PathLike) -> NumpyArrayDumper:
        sources_saver = NumpyArrayDumper(
            dir=tmp_dir,
            filename=create_unique_file(
                dir=tmp_dir,
                prefix="sources",
                suffix=".npy",
            ).name,
        )
        with sources_saver.open("w"):
            sources_saver.write_data(sources)
        return sources_saver

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

    def _create_reducers(self) -> list[tuple[Process, Connection]]:
        reducers: list[tuple[Process, Connection]] = []
        for ai_cond_idx, sample_saver in enumerate(self.samples_savers):
            with sample_saver.open("r"):
                for state_idx in range(sample_saver.saved_arrays_count):
                    state_saver = self._create_state_saver(ai_cond_idx, state_idx, sample_saver.read_data())
                    for_manager, for_reducer = multiprocessing.Pipe(duplex=True)

                    reducer = Reducer(
                        model_path=self.model_path,
                        method=self.method,
                        state_saver=state_saver,
                        sources_saver=self._sources_saver,
                        tmp_dir=self.tmp_dir,
                        sem=self._sem,
                        ai_condition_idx=ai_cond_idx,
                        conn=for_reducer,
                    )

                    process_name = f"reduces_for_{ai_cond_idx}_ai_condition_and_{state_idx}_state"
                    process = multiprocessing.Process(target=reducer.run, name=process_name, daemon=True)
                    reducers.append((process, for_manager))
        return reducers

    def open(self) -> None:
        if self._workers_closer is not None:
            raise ValueError("Already opened")
        logger = get_logger()
        logger.debug("Creating reducers")
        self._reducers = self._create_reducers()
        logger.debug("Starting reducers")
        logger.info("Creating matrixes")
        self._workers_closer = WorkersCloser(self._reducers)
        self._workers_closer.open()

    def close(self) -> None:
        if self._workers_closer is None:
            raise ValueError("Not opened")
        logger = get_logger()
        logger.debug("Closing reducers")
        self._workers_closer.close()

    def __enter__(self) -> "ReducersManager":
        self.open()
        return self

    def __exit__(self, *args: Any) -> None:
        self.close()

    def reduce(self, threshold: float) -> set[int]:
        assert self._reducers is not None  # noqa: S101
        logger = get_logger()

        if not self._matrixes_created:
            for __, conn in self._reducers:
                message, details = cast(tuple[Answer, tuple[Any, ...]], conn.recv())
                if message != Answer.MATRIX_CREATED:
                    raise RuntimeError("Error in reducer process. Creating matrix failed")

            logger.info("Matrixes created")

            self._matrixes_created = True

        for __, conn in self._reducers:
            conn.send((Command.REDUCE, (threshold,)))

        retained_species: set[int] = set(self.retained_species)
        for __, conn in self._reducers:
            message, details = cast(tuple[Answer, tuple[Any, ...]], conn.recv())

            if message != Answer.REDUCED:
                raise RuntimeError("Error in reducer process. Reduce failed")

            retained_species_saver = cast(NumpyArrayDumper, details[0])
            with retained_species_saver.open("r"):
                retained_species = retained_species.union(cast(NDArray[np.uintp], retained_species_saver.read_data()))

        return retained_species
