import contextlib
import os
from enum import Enum
from multiprocessing.connection import Connection
from multiprocessing.context import Process
from pathlib import Path
from tempfile import mkdtemp, mkstemp
from typing import Any, BinaryIO, Literal, cast

import numpy as np
from numpy.typing import NDArray

from .typing import PathLike


def create_unique_file(dir: PathLike, prefix: str | None = None, suffix: str | None = None) -> Path:  # noqa: A002
    fd, filepath = mkstemp(dir=dir, prefix=prefix, suffix=suffix)
    with contextlib.suppress(OSError):
        os.close(fd)
    return Path(filepath)


class NumpyArrayDumper:
    def __init__(self, dir: PathLike | None = None, filename: str | None = None) -> None:  # noqa: A002
        self.dirpath = dir
        self.dir_is_tmp = False
        self.filename = filename
        self.filepath: PathLike | None = None
        self.file_is_tmp = False

        self.saved_arrays_count: int = 0

        self._file: BinaryIO | None = None

    def __enter__(self) -> "NumpyArrayDumper":
        return self

    def __exit__(self, *args: Any) -> None:
        self.close()

    def open(self, mode: Literal["r", "w", "w+"] = "w+") -> "NumpyArrayDumper":
        if self.filepath:
            raise ValueError("It has already been opened")
        if self._file:
            raise ValueError("Is is already opened")

        mode_casted = cast(Literal["rb", "wb", "w+b"], mode + "b")

        if not self.dirpath:
            self.dirpath = mkdtemp(prefix="hkreduce_")
            self.dir_is_tmp = True
        self.dirpath = Path(self.dirpath)

        if not self.filename:
            self.filepath = create_unique_file(prefix="hkreduce_", suffix=".npy", dir=self.dirpath)
            self.filename = self.filepath.name
        else:
            self.filepath = self.dirpath / self.filename

        self._file = open(self.filepath, mode=mode_casted) # noqa: SIM115

        return self

    def close(self) -> None:
        if self._file is None:
            raise ValueError("It is not opened")
        self._file.close()
        self._file = None

    def clean(self, *, force_rmfile: bool = False) -> None:
        if self._file is not None:
            raise ValueError("It is opened")

        if self.filepath is None or self.dirpath is None:
            return

        if self.file_is_tmp or force_rmfile:
            with contextlib.suppress(OSError):
                os.remove(self.filepath)

        if not self.dir_is_tmp:
            return
        with contextlib.suppress(OSError):
            os.rmdir(self.dirpath)

    def write_data(self, array: NDArray) -> None:
        if self._file is None:
            raise ValueError("File is not opened or already closed")
        if "w" not in self._file.mode:
            raise ValueError("File is not opened to write")
        np.save(self._file, array, allow_pickle=False)
        self.saved_arrays_count += 1

    def read_data(self) -> NDArray:
        if self._file is None:
            raise ValueError("File is not opened or already closed")
        if "r" not in self._file.mode:
            raise ValueError("File is not opened to read")
        return np.load(
            self._file,
            allow_pickle=False,
        )


class WorkersCloser:
    def __init__(self, workers: list[tuple[Process, Connection]]) -> None:
        self.workers = workers

    def open(self) -> None:
        started_processes: list[Process] = []
        try:
            for proccess, conn in self.workers:
                proccess.start()
                started_processes.append(proccess)
                message, __ = cast(tuple[Enum, tuple[Any, ...]], conn.recv())
                if message.name.lower() != "initialized":
                    raise RuntimeError("Error in worker. It is not initialized")
        except:
            for process in started_processes:
                if process.is_alive():
                    proccess.terminate()
            for process in started_processes:
                if process.is_alive():
                    proccess.join()
            raise

    def close(self) -> None:
        for worker_process, __ in self.workers:
            if worker_process.is_alive():
                worker_process.terminate()

        for worker_process, __ in self.workers:
            if worker_process.is_alive():
                worker_process.join()

    def __enter__(self) -> "WorkersCloser":
        self.open()
        return self

    def __exit__(self, *args: Any) -> None:
        self.close()
