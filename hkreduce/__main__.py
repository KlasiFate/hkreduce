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
) -> Config:
    kwargs: dict[str, Any] = {
        "input": input,
        "debug": debug,
        "tmp_dir": tmp_dir,
        "reducing_task_config": reducing_task_config,
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


def run(model: Solution, config: Config) -> None:
    Main(model=model, config=config).run()


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
    setup_config(debug=debug)
    logger = get_logger()

    logger.info("Start program")

    try:
        reducing_task_config = read_reducing_task_config(input)
    except ValueError as error:
        logger.error(
            f"Error while reading reducing task config. Error msg:\n{error.args[0]}"  # noqa: G004
        )
        return
    except Exception:  # noqa: BLE001
        logger.opt(exception=True).critical("Uncaught error")
        return

    try:
        model = Solution(reducing_task_config.model)
    except ct.CanteraError as error:
        logger.error(f"Problems while loading model.\n{error.args[0]}")  # noqa: G004
        return

    try:
        species = {specy.name for specy in model.species()}
        reducing_task_config.check_all_species_exist(species)
    except ValueError as error:
        logger.error(error.args[0])
        return

    with TemporaryDirectory(
        prefix=f"hkreduce_{reducing_task_config.method.lower()}_reduce_model_with_{model.n_species}_species_",
        cleanup=not debug,
    ) as tmp_dir:
        if debug:
            logger.info("Save all files in `{tmp_dir}` dir. This dir will not removed", tmp_dir=tmp_dir)
        try:
            config = create_config(
                input=input,
                output=output,
                num_threads=num_threads,
                tmp_dir=tmp_dir,
                reducing_task_config=reducing_task_config,
                debug=debug,
            )
        except ValueError as error:
            logger.error(
                f"Error while creating config. Error msg:\n{error.args[0]}"  # noqa: G004
            )
            return
        except Exception:  # noqa: BLE001
            logger.opt(exception=True).critical("Uncaught error")
            return

        try:
            run(model, config)
        except BaseError as error:
            logger.error(error.msg)
            return
        except Exception:  # noqa: BLE001
            logger.opt(exception=True).critical("Uncaught error")
            return
        finally:
            logger.info("Program finished")


if __name__ == "__main__":
    main()
