import os
from contextlib import contextmanager
from tempfile import mkstemp
from typing import Any, BinaryIO, Generator

import cantera as ct
import numpy as np
from numpy.typing import NDArray

from .config import AutoignitionCondition
from .typing import PathLike


class NumpyArrayDumper:
    def __init__(self, file: BinaryIO) -> None:
        self.file: BinaryIO = file

    def write_data(self, time: float, temperature: float, pressure: float, mass_fractions: NDArray[np.float64]) -> None:
        if self.file is None or self.file.closed:
            raise ValueError("File is not opened or already closed")
        if "w" not in self.file.mode:
            raise ValueError("File is not opened to write")
        np.save(
            self.file,
            np.array((time, temperature, pressure, mass_fractions), dtype=np.float64),
            allow_pickle=False,
        )
        np.save(self.file, mass_fractions, allow_pickle=False)

    def read_data(self) -> tuple[float, float, float, NDArray[np.float64]]:
        if self.file is None or self.file.closed:
            raise ValueError("File is not opened or already closed")
        if "r" not in self.file.mode:
            raise ValueError("File is not opened to read")
        time, temperature, pressure = np.load(
            self.file,
            allow_pickle=False,
        )
        mass_fractions = np.load(
            self.file,
            allow_pickle=False,
        )
        return time, temperature, pressure, mass_fractions


class StateLogger:
    def __init__(self, tmp_dir: PathLike) -> None:
        self.max_temperature: float | None = None
        self.logged_steps_count = 0
        self.ignition_delay: float | None = None

        self.tmp_dir = tmp_dir
        self.dumper: NumpyArrayDumper | None = None
        self.filename: PathLike | None = None

    def update(self, time: float, temperature: float, pressure: float, mass_fractions: NDArray[np.float64]) -> None:
        if self.dumper is None:
            raise ValueError("State logger is not opened to write")
        if self.max_temperature is None or self.max_temperature < temperature:
            self.max_temperature = temperature
        self.logged_steps_count += 1
        self.dumper.write_data(time, temperature, pressure, mass_fractions)

    def read_step_data(self) -> tuple[float, float, float, NDArray[np.float64]]:
        if self.dumper is None:
            raise ValueError("State logger is not opened to read")
        return self.dumper.read_data()

    def set_ignition_delay(self, ignition_delay: float) -> None:
        self.ignition_delay = ignition_delay

    @contextmanager
    def open_to_write(self) -> Generator["StateLogger", Any, Any]:
        file_descriptor, self.filename = mkstemp(prefix="hkreduce_", dir=self.tmp_dir)
        try:
            with open(file_descriptor, mode="wb") as file:
                self.dumper = NumpyArrayDumper(file)
                yield self
        finally:
            # I don't know I should close fd or not
            os.close(file_descriptor)
            self.dumper = None

    @contextmanager
    def open_to_read(self) -> Generator["StateLogger", Any, Any]:
        try:
            with open(self.filename, mode="rb") as file:
                self.dumper = NumpyArrayDumper(file)
                yield self
        finally:
            self.dumper = None


class Simulation:
    def __init__(self, model: ct.Solution, condition: AutoignitionCondition, tmp_dir: PathLike) -> None:
        self.model = model
        self.condition = condition
        self.tmp_dir = tmp_dir

    def run(self) -> StateLogger:
        self.model.TPX = (self.condition.temperature, self.condition.pressure * ct.one_atm, self.condition.reactants)
        if self.condition.kind == "CONSTANT_PRESSURE":
            reactor = ct.IdealGasConstPressureReactor(self.model)
        else:
            reactor = ct.IdealGasReactor(self.model)
        simulation = ct.ReactorNet([reactor])

        if self.condition.max_time_step:
            simulation.max_time_step = self.condition.max_time_step
        end_of_time = self.condition.end_of_time

        state_logger = StateLogger(self.tmp_dir)
        ignition_delay: float | None = None

        max_state_values = simulation.get_state()
        residual_threshold = self.condition.residual_threshold_coef * simulation.rtol
        absolute_tolerance = simulation.atol

        with state_logger.open_to_write():
            step_idx = 0
            while step_idx < self.condition.max_steps and (end_of_time is None or simulation.time < end_of_time):
                prev_state = simulation.get_state()

                simulation.step()

                state_logger.update(simulation.time, reactor.T, reactor.thermo.P, reactor.Y)

                if ignition_delay is None and self.condition.temperature + 400.0 <= reactor.T:
                    ignition_delay = simulation.time

                current_state = simulation.get_state()
                max_state_values = np.maximum(max_state_values, current_state)

                residual = np.linalg.norm(
                    (current_state - prev_state) / (max_state_values + absolute_tolerance)
                ) / np.sqrt(self.sim.n_vars)

                if residual < residual_threshold:
                    break

            if ignition_delay:
                state_logger.set_ignition_delay(ignition_delay)

        return state_logger
