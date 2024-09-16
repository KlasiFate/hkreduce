import multiprocessing
import os
from pathlib import Path
from typing import Any

import cantera as ct  # type: ignore[import-untyped]
import click
import yaml
import yaml.error
from cantera import Solution
from pydantic import ValidationError

from .config import Config, ReducingTaskConfig
from .errors import BaseError
from .logging import get_logger, setup_config
from .main import Main


def create_config(input: str, output: str | None, num_threads: int | None, debug: bool) -> Config:  # noqa: A002, FBT001
    kwargs: dict[str, Any] = {"input": input, "debug": debug}
    if output is not None:
        kwargs["output"] = output
    if num_threads is not None:
        kwargs["num_threads"] = num_threads

    try:
        return Config(**kwargs)
    except ValidationError as error:
        suberror = error.errors(include_context=False, include_input=True)[0]
        raise ValueError(suberror["msg"]) from error


def read_reducing_task_config(config: Config) -> ReducingTaskConfig:
    default_msg = f"Wrong format of yaml file `{config.input}`."
    try:
        with open(config.input, "rt") as file:
            data = yaml.safe_load(file)
    except OSError as error:
        raise ValueError(
            f"Can't read file `{config.input}` cause: `{error.strerror}`, errorno: {error.errno}"
        ) from error
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
        task.model = config.input.parent / task.model
    return task


def run(model: Solution, config: Config, reducing_task_config: ReducingTaskConfig) -> None:
    Main(
        model=model,
        reducing_task_config=reducing_task_config,
        num_threads=config.num_threads,
        output_model_path=config.output,
    ).run()


@click.command()
@click.option(
    "--input",
    required=True,
    type=str,
    help="Path to an yaml file that describes reducing task",
)
@click.option(
    "--output",
    required=False,
    type=str,
    help="Path to an yaml file that describes reducing task",
)
@click.option(
    "--num-threads",
    required=False,
    type=int,
    help="Count of cpu cores used to execute parallel simulations and to reduce graphes. \
Default: n - 1 if system is mutlicores else 1",
)
@click.option(
    "--debug",
    required=False,
    is_flag=True,
    default=False,
    help="Enable debug mode that make logs more verbose",
)
def main(input: str, output: str | None, num_threads: int | None, debug: bool) -> None:  # noqa: A002,FBT001
    multiprocessing.set_start_method(method="spawn")
    setup_config(debug=debug)
    logger = get_logger()
    try:
        config = create_config(input, output, num_threads, debug=debug)
        reducing_task_config = read_reducing_task_config(config)
    except ValueError as error:
        logger.error(
            f"Error while reading creating config or reading reducing task config. Error msg:\n{error.args[0]}"  # noqa: G004
        )
        logger.complete()
        return
    except Exception:  # noqa: BLE001
        logger.opt(exception=True).critical("Uncaught error")
        logger.complete()
        return

    try:
        model = Solution(reducing_task_config.model)
    except ct.CanteraError as error:
        logger.error(f"Problems while loading model.\n{error.args[0]}")  # noqa: G004
        logger.complete()
        return

    try:
        species = {specy.name for specy in model.species()}
        reducing_task_config.check_all_species_exist(species)
    except ValueError as error:
        logger.error(error.args[0])
        logger.complete()
        return

    try:
        run(model, config, reducing_task_config)
    except BaseError as error:
        logger.error(error.msg)
        return
    except Exception:  # noqa: BLE001
        logger.opt(exception=True).critical("Uncaught error")
        return
    finally:
        logger.complete()


if __name__ == "__main__":
    main()
