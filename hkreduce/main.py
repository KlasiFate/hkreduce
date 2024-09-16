# ruff: noqa: G004
import shutil
import time
from tempfile import TemporaryDirectory
from typing import Any, cast

import numpy as np
from cantera import Solution  # type: ignore[import-untyped]
from numpy.typing import NDArray

from .config import ReducingTaskConfig
from .errors import AutoretrievingInitialThresholdError, ReducingError, SimulationError
from .logging import get_logger
from .reducing import ReducersManager
from .simulation import SimulationManager
from .typing import PathLike
from .utils import create_unique_file


class Main:
    def __init__(
        self,
        model: Solution,
        reducing_task_config: ReducingTaskConfig,
        num_threads: int,
        output_model_path: PathLike,
    ) -> None:
        self.model = model
        self.reducing_task_config = reducing_task_config
        self.num_threads = num_threads
        self.output_model_path = output_model_path

        self._sources = self._get_species_indexes(reducing_task_config.target_species, model)
        self._retained_species = self._get_species_indexes(reducing_task_config.retained_species, model)

    @classmethod
    def _get_species_indexes(cls, target_species: list[str], model: Solution) -> NDArray[np.uintp]:
        sources = np.empty((len(target_species)), dtype=np.uintp)
        for i, source in enumerate(target_species):
            sources[i] = model.species_index(source)
        return sources

    def _retrieve_initial_threshold(
        self, reducers_manager: ReducersManager, original_ignition_delays: list[float], tmp_dir: PathLike
    ) -> tuple[float, PathLike, set[int], int, float]:
        logger = get_logger()
        for i in range(self.reducing_task_config.initial_threshold_auto_retrieving_attempts):
            threshold = self.reducing_task_config.initial_threshold * (
                self.reducing_task_config.initial_threshold_auto_retrieving_multiplier**i
            )
            logger.debug(f"Attempt {threshold} as initial threshold")

            logger.info(f"Reduce model with threshold: {threshold}")
            retained_species = reducers_manager.reduce(threshold)
            if len(retained_species) == self.model.n_species:
                logger.info(f"Reducing model doesn't reduce more species with threshold: {threshold}")
                continue
            if len(retained_species) == 0:
                logger.info(f"Reducing model reduce all species with threshold: {threshold}")
                continue

            logger.info(f"Model reduced with {len(retained_species)} species and threshold {threshold}")

            model, model_path = self._create_reduced_model(retained_species, tmp_dir)
            reactions_count = len(model.reactions())
            logger.info(f"Created reduced model with {len(retained_species)} species: `{model_path}`")

            logger.info("Calculating error")
            error = self._calc_error(model_path, original_ignition_delays, tmp_dir)
            logger.info(
                f"Reduced model with {len(retained_species)} species and {reactions_count} \
reactions at `{model_path}` gives: {error * 100}"
            )
            if error < self.reducing_task_config.max_error:
                return threshold, model_path, retained_species, reactions_count, error

        raise AutoretrievingInitialThresholdError("Auto retrieving initial threshold failed because no threshold")

    def _create_reduced_model(self, retained_species: set[int], tmp_dir: PathLike) -> tuple[Solution, PathLike]:
        removed_species = set(range(self.model.n_species))
        removed_species = removed_species.difference(retained_species)
        model_species = self.model.species()
        removed_species_names = {model_species[removed_specy].name for removed_specy in removed_species}

        # TODO: read reactions from file rather than model. Because it is not known reaction modifying has effect or not

        reactions_of_reduced_model: list[Any] = []
        for reaction in self.model.reactions():
            if any(product_name in removed_species_names for product_name in reaction.products):
                continue

            if any(reactant_name in removed_species_names for reactant_name in reaction.reactants):
                continue

            third_body = reaction.third_body
            if third_body:
                retained_efficiencies: dict[str, float] = {}
                for specy_name, efficiency in cast(dict[str, float], third_body.efficiencies).items():
                    if specy_name not in removed_species_names and efficiency != 0.0:
                        retained_efficiencies[specy_name] = efficiency  # noqa: PERF403

                if len(retained_efficiencies) == 0:
                    continue

                third_body.efficiencies = retained_efficiencies

            reactions_of_reduced_model.append(reaction)

        species_of_reduced_model: list[Any] = [
            specy for specy in self.model.species() if specy.name not in removed_species_names
        ]

        model = Solution(
            species=species_of_reduced_model,
            reactions=reactions_of_reduced_model,
            thermo="IdealGas",
            kinetics="GasKinetics",
        )
        model_filename = create_unique_file(
            dir=tmp_dir, prefix=f"reduced_model_with_{len(species_of_reduced_model)}_species", suffix=".yaml"
        )
        model.write_yaml(filename=model_filename)

        return model, model_filename

    def _calc_error(self, model_path: PathLike, original_ignition_delays: list[float], tmp_dir: PathLike) -> float:
        try:
            __, ignition_delays = SimulationManager(
                model_path=model_path,
                reducing_task_config=self.reducing_task_config,
                num_threads=self.num_threads,
                tmp_dir=tmp_dir,
                only_ignition_delays=True,
            ).run()
        except SimulationError:
            return 1

        max_error: float = 0.0
        for i in range(len(original_ignition_delays)):
            error = min(abs(1 - ignition_delays[i] / original_ignition_delays[i]), 1)
            if max_error < error:
                max_error = error
        return max_error

    def run(self) -> None:
        logger = get_logger()
        start = time.time()
        with TemporaryDirectory(prefix="hkreduce_") as tmp_dir:
            logger.info("Start simulations to retrieve samples")
            samples_savers, original_ignition_delays = SimulationManager(
                model_path=self.reducing_task_config.model,
                reducing_task_config=self.reducing_task_config,
                num_threads=self.num_threads,
                tmp_dir=tmp_dir,
                only_ignition_delays=False,
            ).run()
            logger.info("Samples is got")

            with ReducersManager(
                model_path=self.reducing_task_config.model,
                method=self.reducing_task_config.method,
                samples_savers=samples_savers,
                sources=self._sources,
                retained_species=self._retained_species,
                tmp_dir=tmp_dir,
                num_threads=self.num_threads,
            ) as reducers_manager:
                initial_threshold = self.reducing_task_config.initial_threshold

                if not self.reducing_task_config._initial_threshold_set_by_user:  # noqa: SLF001
                    logger.info("Start auto retrieving initial threshold")
                    initial_threshold, model_path, retained_species, retained_reactions_count, error = (
                        self._retrieve_initial_threshold(reducers_manager, original_ignition_delays, tmp_dir)
                    )
                    logger.info(f"Use {initial_threshold} as initial threshold")
                else:
                    logger.info(f"Reduce model with threshold: {initial_threshold}")
                    retained_species = reducers_manager.reduce(initial_threshold)
                    logger.info(f"Model reduced with {len(retained_species)} species and threshold {initial_threshold}")

                    model, model_path = self._create_reduced_model(retained_species, tmp_dir)
                    retained_reactions_count = len(model.reactions())
                    logger.info(f"Created reduced model with {len(retained_species)} species: `{model_path}`")

                    logger.info("Calculating error")
                    error = self._calc_error(model_path, original_ignition_delays, tmp_dir)
                    logger.info(
                        f"Reduced model with {len(retained_species)} species and {retained_reactions_count} \
reactions at `{model_path}` gives: {error * 100}"
                    )

                    if error > self.reducing_task_config.max_error:
                        raise ReducingError("Invalid user initial threshold")

                threshold = initial_threshold
                prev_model_path = model_path
                prev_retained_species_count = len(retained_species)
                prev_retained_reactions_count = retained_reactions_count
                prev_error = error
                while True:
                    if self.reducing_task_config.threshold_increment:
                        threshold += self.reducing_task_config.threshold_increment
                    else:
                        threshold += initial_threshold

                    logger.info(f"Reduce model with threshold: {threshold}")
                    retained_species = reducers_manager.reduce(threshold)
                    if len(retained_species) == prev_retained_species_count:
                        logger.info(f"Reducing model doesn't reduce more species with threshold: {threshold}")
                        continue

                    logger.info(f"Model reduced with {len(retained_species)} species and threshold {threshold}")

                    model, model_path = self._create_reduced_model(retained_species, tmp_dir)
                    retained_reactions_count = len(model.reactions())
                    logger.info(
                        f"Created reduced model with {len(retained_species)} species and \
{retained_reactions_count} reactions: `{model_path}`"
                    )

                    logger.info("Calculating error")
                    error = self._calc_error(model_path, original_ignition_delays, tmp_dir)
                    if error > self.reducing_task_config.max_error:
                        logger.info(
                            f"Reduced model with {len(retained_species)} species and {retained_reactions_count} \
reactions at `{model_path}` gives too large error"
                        )
                        break

                    prev_model_path = model_path
                    prev_retained_species_count = len(retained_species)
                    prev_retained_reactions_count = retained_reactions_count
                    prev_error = error

                    logger.info(
                        f"Reduced model with {len(retained_species)} species and {retained_reactions_count} \
at `{model_path}` gives error: {error * 100}"
                    )

                logger.info(f"Use model with {prev_retained_species_count} species and {prev_retained_reactions_count} \
at `{prev_model_path}` as result")
                logger.info("Copying model to output path")
                try:
                    shutil.copyfile(model_path, self.output_model_path)
                except Exception as error:
                    raise RuntimeError(f"Failed to copy result reduced model to {self.output_model_path}") from error

                stop = time.time()
                logger.info(
                    f"Stats: time: {round(stop - start, 6)} species: {prev_retained_species_count} \
reactions: {prev_retained_reactions_count} error: {prev_error * 100}"
                )
