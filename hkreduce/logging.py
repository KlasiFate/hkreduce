import sys

from loguru import Logger, logger

from .typing import LoggingConfig


def get_config(*, debug: bool = False) -> LoggingConfig:
    format = "{time:YYYY-MM-DDTHH:mm:ss.SSSSSS} - <level>{level:<9}</level> - {message}"  # noqa: A001

    level = "INFO"
    if debug:
        level = "DEBUG"

    return {"handlers": {"sink": sys.stdout, "format": format, "colorize": True, "diagnostic": debug, "level": level}}


_debug: bool = False
_config_setup: bool = False


def setup_config(config: LoggingConfig | None = None, *, debug: bool = False) -> None:
    global _debug, _config_setup
    _debug = debug
    if config is None:
        config = get_config(debug=debug)
    logger.configure(**config)
    _config_setup = True


def get_logger() -> Logger:
    if not _config_setup:
        setup_config()
    if _debug:
        return logger.opt(exception=True)
    return logger
