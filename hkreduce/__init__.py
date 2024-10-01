import contextlib
import multiprocessing

with contextlib.suppress(RuntimeError):
    # for child process context is set by parent
    multiprocessing.set_start_method(method="spawn")
