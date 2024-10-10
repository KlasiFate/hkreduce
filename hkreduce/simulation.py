import multiprocessing
from contextlib import contextmanager
from enum import Enum, auto
from multiprocessing.synchronize import BoundedSemaphore
from typing import Any, Generator, cast

import numpy as np
from cantera import (  # type: ignore[import-untyped]
    IdealGasConstPressureReactor,
    IdealGasReactor,
    ReactorNet,
    one_atm,
)
from loguru import logger
from numpy.typing import NDArray

from .config import Config
from .errors import NoAutoignitionError, SampleCreatingError, TooSmallStepsSampleError
from .typing import AmountDefinitionType, PathLike
from .utils import NumpyArrayDumper, Worker, WorkersManager, create_unique_file, load_model


class Answer(int, Enum):
    SAMPLE_ERROR = auto()
    ERROR = auto()
    END = auto()


class StateLogger:
    def __init__(self, tmp_dir: PathLike, ai_condition_idx: int, n_species: int) -> None:
        self.max_temperature: float | None = None
        self.logged_steps_count = 0
        self.ignition_delay: float | None = None
        self.ignition_temperature: float | None = None
        self.ai_condition_idx = ai_condition_idx
        self.tmp_dir = tmp_dir
        self.n_species = n_species

        self._dumper: NumpyArrayDumper | None = None

    def update(self, time: float, temperature: float, pressure: float, mass_fractions: NDArray[np.float64]) -> None:
        if self._dumper is None:
            raise ValueError("Not have been opened to write")
        if self.max_temperature is None or self.max_temperature < temperature:
            self.max_temperature = temperature
        self.logged_steps_count += 1
        array = np.concatenate((np.array([time, temperature, pressure], dtype=np.float64), mass_fractions), axis=0)
        self._dumper.write_data(array)

    def read_step_data(self) -> tuple[float, float, float, NDArray[np.float64]]:
        if self._dumper is None:
            raise ValueError("Not have been opened to read")
        array = self._dumper.read_data()
        time, temperature, pressure = array[:3:]
        return time, temperature, pressure, array[3::]

    def set_ignition_delay_and_temperature(self, ignition_delay: float, temperature: float) -> None:
        self.ignition_delay = ignition_delay
        self.ignition_temperature = temperature

    @contextmanager
    def open_to_write(self) -> Generator["StateLogger", Any, Any]:
        try:
            filename = create_unique_file(
                suffix=".npy",
                prefix=f"steps_cache_of_{self.ai_condition_idx}_ai_cond_for_model_with_{self.n_species}_species_",
                dir=self.tmp_dir,
            ).name
            self._dumper = NumpyArrayDumper(dir=self.tmp_dir, filename=filename)
            self._dumper.open("w")

            yield self
        finally:
            self._dumper.close()

    @contextmanager
    def open_to_read(self) -> Generator["StateLogger", Any, Any]:
        if self._dumper is None:
            raise ValueError("Not have been opened to write")
        try:
            self._dumper.open("r")
            yield self
        finally:
            self._dumper.close()


class Simulation(Worker):
    def __init__(
        self,
        config: Config,
        model_path: PathLike,
        ai_condition_idx: int,
        sem: BoundedSemaphore,
        *,
        only_ignition_delay: bool,
    ) -> None:
        super().__init__(name=f"simulation_for_{ai_condition_idx}_ai_condition")

        self.config = config
        self.model_path = model_path
        self.ai_condition_idx = ai_condition_idx
        self.sem = sem
        self.only_ignition_delay = only_ignition_delay
        self.logger = logger

    def _simulate(self) -> StateLogger:  # noqa: C901
        model = load_model(self.model_path)

        ai_condition = self.config.reducing_task_config.autoignition_conditions[self.ai_condition_idx]

        if ai_condition.equivalence_ratio:
            model.TP = (
                ai_condition.temperature,
                ai_condition.pressure * one_atm,
            )
            model.set_equivalence_ratio(
                ai_condition.equivalence_ratio,
                ai_condition.fuel,
                ai_condition.oxidizer,
                basis="mole" if ai_condition.amount_definition_type == AmountDefinitionType.MOLES_FRACTIONS else "mass",
            )
        elif ai_condition.amount_definition_type == AmountDefinitionType.MOLES_FRACTIONS:
            model.TPX = (
                ai_condition.temperature,
                ai_condition.pressure * one_atm,
                ai_condition.reactants,
            )
        else:
            model.TPY = (
                ai_condition.temperature,
                ai_condition.pressure * one_atm,
                ai_condition.reactants,
            )

        if self.config.verbose >= 3:
            logger.trace("Save net stoich coefs")
            with NumpyArrayDumper(
                dir=self.config.tmp_dir,
                filename=create_unique_file(
                    dir=self.config.tmp_dir,
                    prefix=f"reactants_stoich_coefs_of_model_with_{model.n_species}_species_",
                    suffix=".npy",
                ).name,
            ).open("w") as saver:
                saver.write_data(model.reactant_stoich_coeffs)
            with NumpyArrayDumper(
                dir=self.config.tmp_dir,
                filename=create_unique_file(
                    dir=self.config.tmp_dir,
                    prefix=f"products_stoich_coefs_of_model_with_{model.n_species}_species_",
                    suffix=".npy",
                ).name,
            ).open("w") as saver:
                saver.write_data(model.product_stoich_coeffs)

        if ai_condition.kind == "CONSTANT_PRESSURE":
            reactor = IdealGasConstPressureReactor(model)
        else:
            reactor = IdealGasReactor(model)
        reactor.syncState()
        simulation = ReactorNet([reactor])
        simulation.reinitialize()

        if ai_condition.max_time_step:
            simulation.max_time_step = ai_condition.max_time_step
        end_of_time = ai_condition.end_of_time

        residual_threshold = ai_condition.residual_threshold_coef * simulation.rtol

        state_logger: StateLogger = StateLogger(self.config.tmp_dir, self.ai_condition_idx, n_species=model.n_species)

        def sim() -> None:
            ignition_delay: float | None = None
            ignition_temperature: float | None = None

            max_state_values = simulation.get_state()

            if not self.only_ignition_delay:
                state_logger.update(simulation.time, reactor.T, reactor.thermo.P, reactor.Y)

            step_idx = 0
            while step_idx < ai_condition.max_steps and (end_of_time is None or simulation.time < end_of_time):
                prev_state = simulation.get_state()

                simulation.step()

                if not self.only_ignition_delay:
                    state_logger.update(simulation.time, reactor.T, reactor.thermo.P, reactor.Y)

                if ai_condition.temperature + 400.0 <= reactor.T and ignition_delay is None:
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
        ai_condition = self.config.reducing_task_config.autoignition_conditions[self.ai_condition_idx]

        if state_logger.ignition_delay is None or state_logger.ignition_temperature is None:
            msg = f"No auto ignition happened for {self.ai_condition_idx} case"
            self.logger.error(msg)
            raise NoAutoignitionError(msg)

        temperature_diff = state_logger.ignition_temperature - ai_condition.temperature

        temperature_delta = temperature_diff / ai_condition.steps_sample_size

        filename = create_unique_file(
            self.config.tmp_dir, prefix=f"steps_sample_of_{self.ai_condition_idx}_ai_case_", suffix=".npy"
        ).name

        with (
            state_logger.open_to_read(),
            NumpyArrayDumper(
                dir=self.config.tmp_dir,
                filename=filename,
            ).open("w") as sample_saver,
        ):
            i = 0
            for __ in range(state_logger.logged_steps_count):
                __, temperature, pressure, mass_fractions = state_logger.read_step_data()  # type: ignore[assignment]
                if temperature >= ai_condition.temperature + (i + 1) * temperature_delta:
                    data = np.concatenate((np.array((temperature, pressure), dtype=np.float64), mass_fractions), axis=0)
                    sample_saver.write_data(data)
                    i += 1
                    if i == 20:
                        break

        if i < ai_condition.steps_sample_size:
            msg = f"Too small steps sample is got for {self.ai_condition_idx} case. \
Change steps sample size or case conditions"
            self.logger.error(msg)
            raise TooSmallStepsSampleError(msg)

        return sample_saver

    def _target_to_run(self) -> None:
        try:
            with self.sem:
                self.logger.debug("Run simulation for {ai_condition_idx} case", ai_condition_idx=self.ai_condition_idx)
                state_logger = self._simulate()
                if self.only_ignition_delay:
                    self.logger.debug(
                        "Simulation is finished for {ai_condition_idx} case", ai_condition_idx=self.ai_condition_idx
                    )
                    self._send_msg_to_parent((Answer.END, (state_logger.ignition_delay,)))
                    return

                try:
                    sample = self._create_sample(state_logger)
                    self._send_msg_to_parent((Answer.END, (sample, state_logger.ignition_delay)))
                except SampleCreatingError:
                    self._send_msg_to_parent((Answer.SAMPLE_ERROR, ()))
                    return
                self.logger.debug(
                    "Simulation is finished for {ai_condition_idx} case", ai_condition_idx=self.ai_condition_idx
                )
        except KeyboardInterrupt:
            self.logger.info(
                "Cancelling simulation process for {ai_condition_idx} case", ai_condition_idx=self.ai_condition_idx
            )
        except BaseException as error:
            self.logger.opt(exception=error).critical(
                "Error while simulation for {ai_condition_idx} case", ai_condition_idx=self.ai_condition_idx
            )
            self._send_msg_to_parent((Answer.ERROR, ()))
            if not isinstance(error, Exception):
                raise
        finally:
            self.logger.trace(
                "Complete logger for simulation {ai_condition_idx} case", ai_condition_idx=self.ai_condition_idx
            )
            self.logger.complete()


class SimulationManager(WorkersManager):
    def __init__(
        self,
        model_path: PathLike,
        config: Config,
        *,
        only_ignition_delays: bool,
    ) -> None:
        self.model_path = model_path
        self.config = config
        self.only_ignition_delays = only_ignition_delays

        self._sem = multiprocessing.BoundedSemaphore(config.num_threads)

        super().__init__(self._create_simulations())  # type: ignore[arg-type]

    def _create_simulations(self) -> list[Simulation]:
        simulations: list[Simulation] = []
        for ai_cond_idx in range(len(self.config.reducing_task_config.autoignition_conditions)):
            simulations.append(  # noqa: PERF401
                Simulation(
                    config=self.config,
                    model_path=self.model_path,
                    ai_condition_idx=ai_cond_idx,
                    sem=self._sem,
                    only_ignition_delay=self.only_ignition_delays,
                )
            )
        return simulations

    def run(self) -> tuple[list[NumpyArrayDumper], list[float]]:
        with self:
            samples_savers: list[NumpyArrayDumper] = []
            ignition_delays: list[float] = []
            for i, simulation in enumerate(self.workers):
                message, details = cast(tuple[Answer, tuple[Any, ...]], simulation.get_msg_from_worker())
                if message == Answer.SAMPLE_ERROR:
                    self.logger.info(
                        "No auto ignition detected or too small sample size for {ai_cond_idx} case", ai_cond_idx=i
                    )
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

            for simulation in self.workers:
                simulation.join(self.join_timeout_after_termination)
                if simulation.is_alive():
                    self.logger.error("Simulation worker process is still alive")

        return samples_savers, ignition_delays
