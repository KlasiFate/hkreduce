import atexit
import contextlib
import multiprocessing
import multiprocessing.connection
import os
import threading
import time
import weakref
from abc import ABC, abstractmethod
from multiprocessing.connection import Connection
from pathlib import Path
from queue import Empty, Queue
from tempfile import TemporaryDirectory as OriginalTemporaryDirectory
from tempfile import mkdtemp, mkstemp
from time import sleep
from typing import Any, BinaryIO, Literal, NamedTuple, cast

import cantera as ct  # type: ignore[import-untyped]
import numpy as np
from cantera import Solution
from loguru import logger
from numpy.typing import NDArray

from .typing import PathLike


def create_unique_file(dir: PathLike, prefix: str | None = None, suffix: str | None = None) -> Path:  # noqa: A002
    fd, filepath = mkstemp(dir=dir, prefix=prefix, suffix=suffix)
    with contextlib.suppress(OSError):
        os.close(fd)
    return Path(filepath)


class NumpyArrayDumper:
    def __init__(self, dir: PathLike | None = None, filename: str | None = None, *, rm: bool = True) -> None:  # noqa: A002
        self.dirpath = dir
        self.dir_is_tmp = False
        self.filename = filename
        self.filepath: PathLike | None = None
        self.file_is_tmp = False
        self.rm = rm

        self.saved_arrays_count: int = 0

        self._file: BinaryIO | None = None

    def __enter__(self) -> "NumpyArrayDumper":
        return self

    def __exit__(self, *args: Any) -> None:
        self.close()

    def open(self, mode: Literal["r", "w", "w+"] = "w+") -> "NumpyArrayDumper":
        if self._file:
            raise ValueError("Is is already opened")
        mode_casted = cast(Literal["rb", "wb", "w+b"], mode + "b")

        if not self.filepath:
            if not self.dirpath:
                self.dirpath = mkdtemp()
                self.dir_is_tmp = True
            self.dirpath = Path(self.dirpath)

            if not self.filename:
                self.filepath = create_unique_file(suffix=".npy", dir=self.dirpath)
                self.filename = self.filepath.name
            else:
                self.filepath = self.dirpath / self.filename

        self._file = open(self.filepath, mode=mode_casted)  # noqa: SIM115

        return self

    def close(self) -> None:
        if self._file is None:
            raise ValueError("It is not opened")
        self._file.close()
        self._file = None

    def clean(self) -> None:
        if self._file is not None:
            raise ValueError("It is opened")

        if self.filepath is None or self.dirpath is None:
            return

        if self.file_is_tmp and self.rm:
            with contextlib.suppress(OSError):
                os.remove(self.filepath)

        if not (self.dir_is_tmp and self.rm):
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


MessageType = Literal["started", "finished", "other"]


class Message(NamedTuple):
    type: MessageType
    args: Any


DEFAULT_POLL_TIMEOUT = 0.001


class Shifter(threading.Thread):
    WAIT_TIMEOUT = 0.5

    def __init__(self, poll_timeout: float = DEFAULT_POLL_TIMEOUT) -> None:
        super().__init__(daemon=True, name="worker_connections_shifter")

        self.poll_timeout = poll_timeout
        self._conns: dict[Connection, tuple[Queue, threading.Event]] = {}
        self._lock = threading.Lock()

        self._closed = False

        self._closer = weakref.finalize(self, self.close)

    def run(self) -> None:
        while not self._closed:
            with self._lock:
                conns = self._conns.copy()
            if not conns:
                sleep(self.poll_timeout)
                continue

            ready_conns = cast(
                list[Connection], multiprocessing.connection.wait(conns.keys(), timeout=self.WAIT_TIMEOUT)
            )

            to_remove: list[Connection] = []

            for conn in ready_conns:
                if conn.closed:
                    conns[conn][1].set()
                    to_remove.append(conn)
                    continue

                queue = conns[conn][0]
                while conn.poll(0):
                    queue.put(conn.recv())

            with self._lock:
                for conn in to_remove:
                    self._conns.pop(conn, None)

    def add(self, conn: Connection, queue: Queue, closed_event: threading.Event) -> None:
        with self._lock:
            if conn in self._conns:
                raise ValueError("Already added")
            self._conns[conn] = (queue, closed_event)

    def remove(self, conn: Connection) -> None:
        with self._lock:
            self._conns.pop(conn, None)

    def close(self) -> None:
        self._closed = True


# this is not in worker class to avoid pickling of parent conn
_shifter: Shifter | None = None
_shifter_install_lock = threading.Lock()


class NotPickableContainer:
    def __init__(self, **kwargs: Any):
        for name, attr in kwargs:
            setattr(name, attr)

    def __getstate__(self) -> None:
        return None

    def __setstate__(self, state: None) -> None:
        return


class Worker(ABC, multiprocessing.Process):
    @classmethod
    def _start_shifter_if_necessary(cls) -> None:
        global _shifter
        if not _shifter:
            with _shifter_install_lock:
                if not _shifter:
                    _shifter = Shifter()
                    _shifter.start()
                    atexit.register(_shifter.close)

    _parent_conn: Connection
    _conns_pair_closed_by_worker: threading.Event
    _parent_queue: Queue

    def __init__(
        self,
        name: str | None = None,
        *,
        daemon: bool = True,
    ) -> None:
        super().__init__(name=name, daemon=daemon)

        parent_conn, self._worker_conn = multiprocessing.Pipe()

        self._not_pickable = NotPickableContainer(
            _parent_conn=parent_conn, _conns_pair_closed_by_worker=threading.Event(), _parent_queue=Queue()
        )

    def __getattr__(self, name: str) -> Any:
        return getattr(self._not_pickable, name)

    def get_msg_from_worker(self, timeout: float | None = None) -> Any:
        deadline: float | None = None
        if timeout is not None and timeout > 0:
            deadline = time.time() + timeout

        while deadline is None or time.time() < deadline:
            try:
                msg = self._parent_queue.get(block=True, timeout=DEFAULT_POLL_TIMEOUT)
                break
            except Empty as error:
                if not self._conns_pair_closed_by_worker.is_set():
                    continue
                raise ValueError(
                    "Workers has already finished (but maybe alive yet) and it didn't send messages. \
Most likely it done job."
                ) from error
        else:
            raise TimeoutError("No messages")

        if not isinstance(msg, Message):
            raise RuntimeError("Unknown msg is received")  # noqa: TRY004
        if msg.type == "started":
            raise RuntimeError('Received "started" type message')
        if msg.type == "finished":
            raise ValueError(
                "Workers has already finished (but maybe alive yet) and it didn't send messages. \
Most likely it done job."
            )

        return msg.args

    def send_to_worker(self, obj: Any) -> None:
        self._parent_conn.send(Message("other", obj))

    def _send_msg_to_parent(self, obj: Any) -> None:
        self._worker_conn.send(Message("other", obj))

    def _get_msg_from_parent(self, timeout: float | None = None) -> Any:
        if not self._worker_conn.poll(timeout):
            raise TimeoutError("No messages")
        msg = self._worker_conn.recv()
        if not isinstance(msg, Message):
            raise RuntimeError("Unknown msg is received")  # noqa: TRY004
        if msg.type != "other":
            raise RuntimeError('Received not "other" type message')
        return msg.args

    def start(self) -> None:
        super().start()

        self._start_shifter_if_necessary()

        assert _shifter  # noqa: S101
        _shifter.add(self._parent_conn, self._parent_queue, closed_event=self._conns_pair_closed_by_worker)

        msg: Any = None
        while msg is None:
            try:
                msg = self._parent_queue.get(block=True, timeout=DEFAULT_POLL_TIMEOUT)
            except Empty as error:  # noqa: PERF203
                if self._conns_pair_closed_by_worker.is_set():
                    raise RuntimeError('No "started" msg received') from error

        if not isinstance(msg, Message) or msg.type != "started":
            raise RuntimeError('Not "started" msg is received')

    @abstractmethod
    def _target_to_run(self) -> Any:
        raise NotImplementedError

    def run(self) -> None:
        try:
            self._worker_conn.send(Message("started", None))
            self._target_to_run()
        finally:
            self._worker_conn.send(Message("finished", None))
            self._worker_conn.close()

    def close(self) -> None:
        super().close()
        if _shifter:
            # Shifter should remove conn and queue but just in case
            _shifter.remove(self._parent_conn)
        self._parent_conn.close()


DEFAULT_JOIN_TIMEOUT_AFTER_TERMINATION = 10


class WorkersManager:
    def __init__(
        self, workers: list[Worker], *, join_timeout_after_termination: float = DEFAULT_JOIN_TIMEOUT_AFTER_TERMINATION
    ) -> None:
        self.workers = workers
        self.opened = False
        self.closed = False
        self.join_timeout_after_termination = join_timeout_after_termination
        self.logger = logger

    def open(self) -> None:
        if self.opened or self.closed:
            raise ValueError("Invalid state")
        for worker in self.workers:
            worker.start()
        self.opened = True

    def close(self, finishing_timeout: float | None = 0) -> None:
        if not self.opened or self.closed:
            raise ValueError("Invalid state")
        for worker in self.workers:
            if not worker.is_alive():
                worker.close()
                continue

            if finishing_timeout == 0:
                self.logger.warning("Worker is still alive. It will be terminated")
                worker.terminate()
            else:
                worker.join(finishing_timeout)
                if worker.is_alive():
                    self.logger.warning("Worker is still alive although it has been waited to finishing")
                    worker.terminate()

            worker.join(self.join_timeout_after_termination)
            if worker.is_alive():
                self.logger.warning("Worker is still alive although it has been terminated")
            else:
                worker.close()
        self.closed = True

    def __enter__(self) -> "WorkersManager":
        self.open()
        return self

    def __exit__(self, *args: Any) -> None:
        self.close()


class TemporaryDirectory(OriginalTemporaryDirectory):
    def __init__(
        self,
        suffix: str | None = None,
        prefix: str | None = None,
        dir: PathLike | None = None,  # noqa: A002
        *,
        cleanup: bool = True,
    ) -> None:
        if dir is not None:
            dir = str(dir)  # noqa: A001
        super().__init__(suffix=suffix, prefix=prefix, dir=dir, ignore_cleanup_errors=True)
        self.do_cleanup = cleanup
        if not self.do_cleanup:
            self._finalizer.detach()  # type: ignore [attr-defined]

    def cleanup(self) -> None:
        if self.do_cleanup:
            return super().cleanup()
        return None


# Solution objects doesn't support weak refs
class ModelWrapper:
    def __init__(self, model: Solution) -> None:
        self.model = model


_models_cache = weakref.WeakValueDictionary[str, ModelWrapper]()


def load_model(model_path: PathLike) -> Solution:
    if isinstance(model_path, str):
        model_path = Path(model_path)

    cantera_datafiles = [Path(path) for path in ct.list_data_files()]
    try:
        if not any(str(model_path) == datafile.name for datafile in cantera_datafiles):
            model_path = model_path.resolve()
    except OSError as error:
        raise ValueError(f"Failed to load model: `{model_path}`") from error

    key = str(model_path)

    model_wrapper = _models_cache.get(key)
    if model_wrapper is not None:
        return model_wrapper.model

    try:
        model = Solution(key)
    except ct.CanteraError as error:
        raise ValueError(f"Failed to load model: `{model_path}`") from error

    _models_cache[key] = ModelWrapper(model)
    return model


def get_species_indexes(species: list[str], model: Solution) -> NDArray[np.uintp]:
    species_indexes = np.empty((len(species)), dtype=np.uintp)
    for i, source in enumerate(species):
        species_indexes[i] = model.species_index(source)
    return species_indexes
