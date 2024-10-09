import shutil
import time
from typing import Any

from cantera import Solution
from loguru import logger  # type: ignore[import-untyped]

from .config import Config
from .errors import AutoretrievingInitialThresholdError, ReducingError, SimulationError, ThresholdError
from .reducing import ReducersManager
from .simulation import SimulationManager
from .typing import PathLike
from .utils import create_unique_file


class Main:
    def __init__(
        self,
        model: Solution,
        config: Config,
    ) -> None:
        self.model = model
        self.config = config
        self.logger = logger

    def _retrieve_initial_threshold(
        self, reducers_manager: ReducersManager, original_ignition_delays: list[float]
    ) -> tuple[float, PathLike, set[int], int, float]:
        for i in range(self.config.reducing_task_config.initial_threshold_auto_retrieving_attempts):
            threshold = self.config.reducing_task_config.initial_threshold * (
                self.config.reducing_task_config.initial_threshold_auto_retrieving_multiplier**i
            )
            self.logger.debug("Attempt {threshold} as initial threshold", threshold=threshold)

            self.logger.info("Reduce model with threshold: {threshold}", threshold=threshold)
            retained_species = reducers_manager.reduce(threshold)
            if len(retained_species) == self.model.n_species:
                self.logger.info(
                    "Reducing model process doesn't reduce model with threshold: {threshold}", threshold=threshold
                )
                break
            if len(retained_species) == 0:
                self.logger.info(
                    "Reducing model process reduces all species with threshold: {threshold}", threshold=threshold
                )
                continue

            self.logger.info(
                "Model reduced with {retained_species_len} species and threshold {threshold}",
                retained_species_len=len(retained_species),
                threshold=threshold,
            )

            model, model_path = self._create_reduced_model(retained_species)
            reactions_count = len(model.reactions())
            self.logger.info(
                "Created reduced model with {retained_species_len} species: `{model_path}`",
                retained_species_len=len(retained_species),
                model_path=model_path,
            )

            self.logger.info("Calculating error")
            error = self._calc_error(model_path, original_ignition_delays)
            self.logger.info(
                "Reduced model with {retained_species_len} species and {reactions_count} \
reactions at `{model_path}` gives: {error}",
                retained_species_len=len(retained_species),
                reactions_count=reactions_count,
                model_path=model_path,
                error=error * 100,
            )
            if error < self.config.reducing_task_config.max_error:
                return threshold, model_path, retained_species, reactions_count, error

        self.logger.error(
            "Auto retrieving initial threshold failed because no threshold that reduces model and satisfies \
provided error limit"
        )

        raise AutoretrievingInitialThresholdError(
            "Auto retrieving initial threshold failed because no threshold that reduces model and satisfies \
provided error limit"
        )

    def _create_reduced_model(self, retained_species: set[int]) -> tuple[Solution, PathLike]:
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
            if (
                third_body
                and not getattr(third_body, "default_efficiency", 1.0)
                and len(third_body.efficiencies) == 1
                and list(third_body.efficiencies.keys())[0] in removed_species_names
            ):
                continue

            if third_body:
                third_body.efficiencies = {
                    sp: val for sp, val in third_body.efficiencies.items() if sp not in removed_species_names
                }

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
            dir=self.config.tmp_dir,
            prefix=f"reduced_model_with_{len(species_of_reduced_model)}_species_",
            suffix=".yaml",
        )
        model.write_yaml(filename=model_filename)

        return model, model_filename

    def _calc_error(self, model_path: PathLike, original_ignition_delays: list[float]) -> float:
        try:
            __, ignition_delays = SimulationManager(
                config=self.config,
                model_path=model_path,
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
        start = time.time()

        self.logger.info("Start simulations to retrieve samples")
        samples_savers, original_ignition_delays = SimulationManager(
            model_path=self.config.reducing_task_config.model,
            config=self.config,
            only_ignition_delays=False,
        ).run()
        self.logger.info("Simulations are finished. Samples is got")

        with ReducersManager(config=self.config, samples_savers=samples_savers) as reducers_manager:
            initial_threshold = self.config.reducing_task_config.initial_threshold

            if not self.config.reducing_task_config._initial_threshold_set_by_user:  # noqa: SLF001
                self.logger.info("Start auto retrieving initial threshold")
                initial_threshold, model_path, retained_species, retained_reactions_count, error = (
                    self._retrieve_initial_threshold(reducers_manager, original_ignition_delays)
                )
                self.logger.info("Use {threshold} as initial threshold", threshold=initial_threshold)
            else:
                self.logger.info("Reduce model with threshold: {threshold}", threshold=initial_threshold)
                retained_species = reducers_manager.reduce(initial_threshold)
                self.logger.info(
                    "Model reduced with {retained_species_count} species and threshold {threshold}",
                    retained_species_count=len(retained_species),
                    threshold=initial_threshold,
                )
                if len(retained_species) == self.model.n_species:
                    self.logger.info(
                        "Model doesn't reduced with user provided threshold: {threshold}", threshold=initial_threshold
                    )
                    raise ThresholdError("Model doesn't reduced with user provided threshold: {threshold}")

                model, model_path = self._create_reduced_model(retained_species)
                retained_reactions_count = len(model.reactions())
                self.logger.info(
                    "Created reduced model with {retained_species_count} species: `{model_path}`",
                    retained_species_count=len(retained_species),
                    model_path=model_path,
                )

                self.logger.info("Calculating error")
                error = self._calc_error(model_path, original_ignition_delays)
                self.logger.info(
                    "Reduced model with {retained_species_count} species and {retained_reactions_count} \
reactions at `{model_path}` gives: {error}",
                    retained_species_count=len(retained_species),
                    retained_reactions_count=retained_reactions_count,
                    model_path=model_path,
                    error=error * 100,
                )

                if error > self.config.reducing_task_config.max_error:
                    raise ReducingError("Invalid user initial threshold")

            threshold = initial_threshold
            prev_model_path = model_path
            prev_retained_species_count = len(retained_species)
            prev_retained_reactions_count = retained_reactions_count
            prev_error = error
            while True:
                if self.config.reducing_task_config.threshold_increment:
                    threshold += self.config.reducing_task_config.threshold_increment
                else:
                    threshold += initial_threshold

                self.logger.info("Reduce model with threshold: {threshold}", threshold=threshold)
                retained_species = reducers_manager.reduce(threshold)
                if len(retained_species) == prev_retained_species_count:
                    logger.info(
                        "Reducing model doesn't reduce more species with threshold: {threshold}", threshold=threshold
                    )
                    continue

                self.logger.info(
                    "Model reduced with {retained_species_count} species and threshold {threshold}",
                    retained_species_count=len(retained_species),
                    threshold=threshold,
                )

                model, model_path = self._create_reduced_model(retained_species)
                retained_reactions_count = len(model.reactions())
                self.logger.info(
                    "Created reduced model with {retained_species_count} species and \
{retained_reactions_count} reactions: `{model_path}`",
                    retained_species_count=len(retained_species),
                    retained_reactions_count=retained_reactions_count,
                    model_path=model_path,
                )

                self.logger.info("Calculating error")
                error = self._calc_error(model_path, original_ignition_delays)
                if error > self.config.reducing_task_config.max_error:
                    self.logger.info(
                        "Reduced model with {retained_species_count} species and {retained_reactions_count} \
reactions at `{model_path}` gives too large error: {error}",
                        retained_species_count=len(retained_species),
                        retained_reactions_count=retained_reactions_count,
                        model_path=model_path,
                        error=error * 100,
                    )
                    break

                prev_model_path = model_path
                prev_retained_species_count = len(retained_species)
                prev_retained_reactions_count = retained_reactions_count
                prev_error = error

                self.logger.info(
                    "Reduced model with {retained_species_count} species and {retained_reactions_count} \
at `{model_path}` gives error: {error}",
                    retained_species_count=len(retained_species),
                    retained_reactions_count=retained_reactions_count,
                    model_path=model_path,
                    error=error * 100,
                )

            self.logger.info(
                "Use model with {retained_species_count} species and {retained_reactions_count} \
at `{model_path}` as result",
                retained_species_count=prev_retained_species_count,
                retained_reactions_count=prev_retained_reactions_count,
                model_path=prev_model_path,
            )
            self.logger.info("Copying model to output path")
            try:
                shutil.copyfile(model_path, self.config.output)
            except Exception as error:
                raise RuntimeError(f"Failed to copy result reduced model to {self.config.output}") from error

            stop = time.time()
            self.logger.info(
                "Stats: time: {time} species: {retained_species_count} \
reactions: {retained_reactions_count} error: {error}",
                time=round(stop - start, 6),
                retained_species_count=prev_retained_species_count,
                retained_reactions_count=prev_retained_reactions_count,
                error=prev_error * 100,
            )
