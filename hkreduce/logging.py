import sys

from loguru import logger as loguru_logger

from .typing import LoggingConfig


def get_config(*, verbose: int = 0, colorized_logs: bool = True) -> LoggingConfig:
    format = "{time:YYYY-MM-DDTHH:mm:ss.SSSSSS} - <level>{level:<9}</level> - {message}"  # noqa: A001

    level = "INFO"
    if verbose >= 1:
        level = "DEBUG"
    if verbose >= 2:
        level = "TRACE"

    return {
        "handlers": [
            {
                "sink": sys.stdout,
                "format": format,
                "colorize": colorized_logs,
                "diagnose": verbose >= 2,
                "level": level,
                "enqueue": True,
            }
        ]
    }


def setup_config(*, verbose: int = 0, colorized_logs: bool = True) -> None:
    loguru_logger.remove()  # sys.stderr is not picklable
    config = get_config(verbose=verbose, colorized_logs=colorized_logs)
    loguru_logger.configure(**config)
