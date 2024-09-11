import multiprocessing
from contextlib import contextmanager
from enum import Enum, auto
from multiprocessing.connection import Connection
from multiprocessing.context import Process
from multiprocessing.synchronize import BoundedSemaphore
from typing import Any, Generator, cast

import numpy as np
from cantera import (  # type: ignore[import-untyped]
    IdealGasConstPressureReactor,
    IdealGasReactor,
    ReactorNet,
    Solution,
    one_atm,
)
from numpy.typing import NDArray

from .config import AutoignitionConditionConfig, ReducingTaskConfig
from .errors import NoAutoignitionError, SampleCreatingError, TooSmallStepsSampleError
from .logging import get_logger, setup_config
from .typing import PathLike
from .utils import NumpyArrayDumper, WorkersCloser, create_unique_file


class Answer(int, Enum):
    INITIALIZED = auto()
    SAMPLE_ERROR = auto()
    ERROR = auto()
    END = auto()


class StateLogger:
    def __init__(self, tmp_dir: PathLike, ai_condition_idx: int) -> None:
        self.max_temperature: float | None = None
        self.logged_steps_count = 0
        self.ignition_delay: float | None = None
        self.ignition_temperature: float | None = None

        self.ai_condition_idx = ai_condition_idx

        filename = create_unique_file(
            suffix=".npy", prefix=f"steps_cache_of_{self.ai_condition_idx}_ai_case_", dir=tmp_dir
        ).name
        self._dumper = NumpyArrayDumper(dir=tmp_dir, filename=filename)

    def update(self, time: float, temperature: float, pressure: float, mass_fractions: NDArray[np.float64]) -> None:
        if self.max_temperature is None or self.max_temperature < temperature:
            self.max_temperature = temperature
        self.logged_steps_count += 1
        self._dumper.write_data(np.array([time, temperature, pressure], dtype=np.float64))
        self._dumper.write_data(mass_fractions)

    def read_step_data(self) -> tuple[float, float, float, NDArray[np.float64]]:
        time, temperature, pressure = self._dumper.read_data()
        mass_fractions = self._dumper.read_data()
        return time, temperature, pressure, mass_fractions

    def set_ignition_delay_and_temperature(self, ignition_delay: float, temperature: float) -> None:
        self.ignition_delay = ignition_delay
        self.ignition_temperature = temperature

    @contextmanager
    def open_to_write(self) -> Generator["StateLogger", Any, Any]:
        try:
            self._dumper.open("w")
            yield self
        finally:
            self._dumper.close()

    @contextmanager
    def open_to_read(self) -> Generator["StateLogger", Any, Any]:
        try:
            self._dumper.open("r")
            yield self
        finally:
            self._dumper.close()


class Simulation:
    def __init__(
        self,
        model_path: PathLike,
        ai_condition: AutoignitionConditionConfig,
        ai_condition_idx: int,
        tmp_dir: PathLike,
        sem: BoundedSemaphore,
        conn: Connection,
        *,
        debug: bool,
        only_ignition_delay: bool,
    ) -> None:
        self.model_path = model_path
        self.ai_condition = ai_condition
        self.ai_condition_idx = ai_condition_idx
        self.tmp_dir = tmp_dir
        self.sem = sem
        self.conn = conn
        self.debug = debug
        self.only_ignition_delay = only_ignition_delay

    def _run_simulation(self) -> StateLogger:
        model = Solution(self.model_path)
        model.TPX = (
            self.ai_condition.temperature,
            self.ai_condition.pressure * one_atm,
            self.ai_condition.reactants,
        )
        if self.ai_condition.kind == "CONSTANT_PRESSURE":
            reactor = IdealGasConstPressureReactor(self.model_path)
        else:
            reactor = IdealGasReactor(self.model_path)
        simulation = ReactorNet([reactor])

        if self.ai_condition.max_time_step:
            simulation.max_time_step = self.ai_condition.max_time_step
        end_of_time = self.ai_condition.end_of_time

        residual_threshold = self.ai_condition.residual_threshold_coef * simulation.rtol

        state_logger: StateLogger = StateLogger(self.tmp_dir, self.ai_condition_idx)

        def sim() -> None:
            ignition_delay: float | None = None
            ignition_temperature: float | None = None

            max_state_values = simulation.get_state()

            if not self.only_ignition_delay:
                state_logger.update(simulation.time, reactor.T, reactor.thermo.P, reactor.Y)

            step_idx = 0
            while step_idx < self.ai_condition.max_steps and (end_of_time is None or simulation.time < end_of_time):
                prev_state = simulation.get_state()

                simulation.step()

                if not self.only_ignition_delay:
                    state_logger.update(simulation.time, reactor.T, reactor.thermo.P, reactor.Y)

                if self.ai_condition.temperature + 400.0 <= reactor.T and ignition_delay is None:
                    ignition_delay = simulation.time
                    ignition_temperature = reactor.T
                    break

                current_state = simulation.get_state()
                max_state_values = np.maximum(max_state_values, current_state)

                residual = np.linalg.norm(
                    (current_state - prev_state) / (max_state_values + simulation.atol)
                ) / np.sqrt(simulation.n_vars)

                if residual < residual_threshold:
                    break

            if ignition_delay is not None and ignition_temperature is not None:
                state_logger.set_ignition_delay_and_temperature(ignition_delay, ignition_temperature)

        if self.only_ignition_delay:
            sim()
            return state_logger

        with state_logger.open_to_write():
            sim()
        return state_logger

    def _create_sample(self, state_logger: StateLogger) -> NumpyArrayDumper:
        logger = get_logger()
        if state_logger.ignition_delay is None or state_logger.ignition_temperature is None:
            msg = f"No auto ignition happened for {self.ai_condition_idx} case"
            logger.info(msg)
            raise NoAutoignitionError(msg)

        temperature_diff = state_logger.ignition_temperature - self.ai_condition.temperature

        temperature_delta = temperature_diff / self.ai_condition.steps_sample_size

        filename = create_unique_file(
            self.tmp_dir, prefix=f"steps_sample_of_{self.ai_condition_idx}_ai_case_", suffix=".npy"
        ).name

        i = 0
        with (
            state_logger.open_to_read(),
            NumpyArrayDumper(
                dir=self.tmp_dir,
                filename=filename,
            ).open("w") as sample_saver,
        ):
            for __ in range(state_logger.logged_steps_count):
                __, temperature, pressure, mass_fractions = state_logger.read_step_data()  # type: ignore[assignment]
                if temperature >= self.ai_condition.temperature + i * temperature_delta:
                    data = np.array((temperature, pressure), dtype=np.float64) + mass_fractions
                    sample_saver.write_data(data)
                    i += 1

        if i < self.ai_condition.steps_sample_size:
            msg = f"Too small steps sample is got for {self.ai_condition_idx} case. \
Change steps sample size or case conditions"
            logger.info(msg)
            raise TooSmallStepsSampleError(msg)

        return sample_saver

    def run(self) -> None:
        setup_config(debug=self.debug)
        logger = get_logger()

        self.conn.send((Answer.INITIALIZED, ()))

        try:
            with self.sem:
                logger.debug(f"Run simulation for {self.ai_condition_idx} case")
                state_logger = self._run_simulation()
                logger.debug(f"Simulation for {self.ai_condition_idx} case is finished")
                if self.only_ignition_delay:
                    self.conn.send((Answer.END, (state_logger.ignition_delay,)))
                    return

                logger.debug(f"Creating sample for {self.ai_condition_idx} case")
                try:
                    sample = self._create_sample(state_logger)
                except SampleCreatingError:
                    self.conn.send((Answer.SAMPLE_ERROR, ()))
                    return
                self.conn.send((Answer.END, (sample, state_logger.ignition_delay)))
        except Exception:
            self.conn.send((Answer.ERROR, ()))
            logger.opt(exception=True).critical("Error while simulation")
            return


class SimulationManager:
    def __init__(
        self,
        model_path: PathLike,
        reducing_task_config: ReducingTaskConfig,
        num_threads: int,
        tmp_dir: PathLike,
        *,
        debug: bool,
        only_ignition_delays: bool,
    ) -> None:
        self.model_path = model_path
        self.reducing_task_config = reducing_task_config
        self.tmp_dir = tmp_dir
        self.debug = debug
        self.only_ignition_delays = only_ignition_delays

        self._sem = multiprocessing.BoundedSemaphore(num_threads)

    def _create_simulations(self) -> list[tuple[Process, Connection]]:
        simulations: list[tuple[Process, Connection]] = []
        for ai_cond_idx, ai_cond in enumerate(self.reducing_task_config.autoignition_conditions):
            for_manager, for_simulation = multiprocessing.Pipe(duplex=True)

            simulation = Simulation(
                model_path=self.reducing_task_config.model,
                ai_condition=ai_cond,
                ai_condition_idx=ai_cond_idx,
                tmp_dir=self.tmp_dir,
                sem=self._sem,
                conn=for_simulation,
                debug=self.debug,
                only_ignition_delay=self.only_ignition_delays,
            )

            process_name = f"simulation_for_{ai_cond_idx}_ai_condition"
            process = multiprocessing.Process(target=simulation.run, name=process_name, daemon=True)
            simulations.append((process, for_manager))

        return simulations

    def run(self) -> tuple[list[NumpyArrayDumper], list[float]]:
        logger = get_logger()
        logger.debug("Creating simulations")
        simulations = self._create_simulations()
        logger.debug("Starting simulations")
        with WorkersCloser(simulations):
            samples_savers: list[NumpyArrayDumper] = []
            ignition_delays: list[float] = []
            for __, conn in simulations:
                message, details = cast(tuple[Answer, tuple[Any, ...]], conn.recv())
                if message == Answer.SAMPLE_ERROR:
                    raise SampleCreatingError("No auto ignition detected or too small sample size")

                if message != Answer.END:
                    raise RuntimeError("Error in simulation worker process")

                if self.only_ignition_delays:
                    ignition_delay = details[0]
                    ignition_delays.append(ignition_delay)
                else:
                    sample_saver, ignition_delay = details
                    samples_savers.append(sample_saver)
                    ignition_delays.append(ignition_delay)

        logger.debug("Simulations are finished")

        return samples_savers, ignition_delays
