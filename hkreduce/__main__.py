import os
from pathlib import Path
from typing import Any

import cantera as ct  # type: ignore[import-untyped]
import yaml
import yaml.error
from cantera import Solution
from loguru import logger
from pydantic import ValidationError

from .cli import gen_options
from .config import Config, ReducingTaskConfig
from .errors import BaseError
from .logging import setup_config as setup_logging_config
from .main import Main
from .typing import PathLike
from .utils import TemporaryDirectory


def create_config(
    input: str,  # noqa: A002
    output: str | None,
    num_threads: int | None,
    tmp_dir: PathLike,
    reducing_task_config: ReducingTaskConfig,
    *,
    debug: bool,
    colorized_logs: bool,
) -> Config:
    kwargs: dict[str, Any] = {
        "input": input,
        "debug": debug,
        "tmp_dir": tmp_dir,
        "reducing_task_config": reducing_task_config,
        "colorized_logs": colorized_logs,
    }
    if output is not None:
        kwargs["output"] = output
    if num_threads is not None:
        kwargs["num_threads"] = num_threads

    try:
        return Config(**kwargs)
    except ValidationError as error:
        suberror = error.errors(include_context=False, include_input=True)[0]
        raise ValueError(suberror["msg"]) from error


def read_reducing_task_config(input: PathLike) -> ReducingTaskConfig:  # noqa: A002
    if isinstance(input, str):
        input = Path(input)  # noqa: A001

    default_msg = f"Wrong format of yaml file `{input}`."
    try:
        with open(input, "rt") as file:
            data = yaml.safe_load(file)
    except OSError as error:
        raise ValueError(f"Can't read file `{input}` cause: `{error.strerror}`, errorno: {error.errno}") from error
    except yaml.error.YAMLError as error:
        msg = default_msg
        if isinstance(error, yaml.error.MarkedYAMLError):
            msg += f" {str(error).capitalize}"
        raise ValueError(msg) from error

    if not isinstance(data, dict) or any(not isinstance(key, str) for key in data):
        raise ValueError(default_msg)  # noqa: TRY004

    try:
        task = ReducingTaskConfig(**data)
    except ValidationError as error:
        suberror = error.errors(include_context=False, include_input=True)[0]
        raise ValueError(suberror["msg"]) from error

    cantera_datafiles = [Path(path) for path in ct.list_data_files()]
    if any(str(task.model) == datafile.name for datafile in cantera_datafiles):
        return task
    if not os.path.isabs(task.model):
        task.model = input.parent / task.model
    return task


def run(**kwargs: Any) -> None:
    try:
        config = create_config(**kwargs)
    except ValueError as error:
        logger.opt(exception=kwargs["verbose"] >= 2).error(
            "Error while creating config. Error msg:\n{msg}", msg=error.args[0]
        )
        return

    try:
        reducing_task_config = read_reducing_task_config(input)
    except ValueError as error:
        logger.opt(exception=config.verbose >= 2).error(
            "Error while reading reducing task config. Error msg:\n{msg}", msg=error.args[0]
        )
        return

    try:
        model = Solution(reducing_task_config.model)
    except ct.CanteraError as error:
        logger.opt(exception=config.verbose >= 2).error("Problems while loading model.\n{msg}", msg=error.args[0])
        return

    try:
        species = {specy.name for specy in model.species()}
        reducing_task_config.check_all_species_exist(species)
    except ValueError as error:
        logger.opt(exception=config.verbose >= 2).error(error.args[0])
        return

    with TemporaryDirectory(
        prefix=f"hkreduce_{reducing_task_config.method.lower()}_reduce_model_with_{model.n_species}_species_",
        cleanup=config.verbose >= 3,
    ) as tmp_dir:
        config._tmp_dir = tmp_dir  # noqa: SLF001
        config._reducing_task_config = reducing_task_config  # noqa: SLF001

        if config.verbose >= 3:
            logger.trace('All output will be saved to "{tmp_dir}"', tmp_dir=tmp_dir)
        try:
            Main(model=model, config=config).run()
        except BaseError as error:
            logger.opt(exception=config.verbose >= 2).error("Reason of finishing:\n{msg}", msg=error.msg)


@gen_options
def main(**kwargs: Any) -> None:
    setup_logging_config(debug=kwargs["verbose"] > 0, colorized_logs=not kwargs["no_colorized_logs"])
    logger.info("Start program")
    try:
        run()
    except BaseException as error:  # noqa: BLE001
        logger.opt(exception=error).critical("Uncaught error:")
        if not isinstance(error, Exception):
            raise
    finally:
        logger.info("Program finished")


if __name__ == "__main__":
    main()
