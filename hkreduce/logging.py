import sys

from loguru import logger

from .typing import LoggingConfig


def get_config(*, debug: bool = False) -> LoggingConfig:
    format = "{time:YYYY-MM-DDTHH:mm:ss.SSSSSS} - <level>{level:<9}</level> - {message}"  # noqa: A001

    level = "INFO"
    if debug:
        level = "DEBUG"

    return {"handlers": {"sink": sys.stdout, "format": format, "colorize": True, "diagnostic":debug, "level":level}}


def setup_config(config: LoggingConfig | None = None, *, debug: bool = False) -> None:
    if config is None:
        config = get_config(debug)
    logger.configure(**config)
