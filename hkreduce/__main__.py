import os
from pathlib import Path
from typing import Any, cast

import cantera as ct
import click
import yaml
import yaml.error
from pydantic import ValidationError

from .config import Config, ReducingTaskConfig
from .reducer import ReducersManager


def create_config(input: str, output: str | None, num_threads: int | None, debug: bool) -> Config:  # noqa: A002
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


def read_yaml_file(config: Config) -> ReducingTaskConfig:
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

    cantera_datafiles = cast(list[Path], ct.list_data_files())
    if any(str(task.model) == datafile.name for datafile in cantera_datafiles):
        return task
    if not os.path.isabs(task.model):
        task.model = config.input / task.model
    return task


def run(config: Config, reducing_task_config: ReducingTaskConfig) -> None:
    pass


@click.command()
@click.argument(
    "input",
    required=True,
    type=str,
    help="Path to an yaml file that describes reducing task",
)
@click.argument(
    "output",
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
    type=bool,
    default=False,
    help="Enable debug mode that make logs more verbose",
)
def main(input: str, output: str | None, num_threads: int | None, debug: bool) -> None:  # noqa: A002
    try:
        config = create_config(input, output, num_threads, debug=debug)
        reducing_task = read_yaml_file(config)
    except ValueError:
        # TODO: log
        return
    except Exception:
        # TODO: log
        return
