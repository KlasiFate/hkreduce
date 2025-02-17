from multiprocessing import cpu_count
from pathlib import Path
from string import ascii_lowercase
from typing import Annotated, Any, Literal

from pydantic import BaseModel as PydanticBaseModel
from pydantic import ConfigDict, Field, model_validator

from .typing import AmountDefinitionType, PathLike, ReducingMethod


class BaseModel(PydanticBaseModel, validate_assignment=True):
    pass


def alias_generator(field_name: str) -> str:
    if field_name.startswith("_"):
        return field_name
    return field_name.replace("_", "-")


class AutoignitionConditionConfig(BaseModel):
    model_config = ConfigDict(BaseModel.model_config, alias_generator=alias_generator)  # type: ignore[misc]

    kind: Literal["CONSTANT_VOLUME", "CONSTANT_PRESSURE"]
    pressure: float
    temperature: float

    reactants: dict[str, float] | None = None

    fuel: dict[str, float] | None = None
    oxidizer: dict[str, float] | None = None
    equivalence_ratio: float | None = Field(default=None, gt=0.0)

    amount_definition_type: AmountDefinitionType = AmountDefinitionType.MOLES_FRACTIONS

    max_steps: int = Field(default=10 * 1000, gt=0)
    end_of_time: float | None = Field(default=None, gt=0.0)
    max_time_step: float | None = Field(default=None, gt=0.0)
    residual_threshold_coef: float = Field(default=10.0, gt=0.0)
    steps_sample_size: int = Field(default=20, gt=0)

    @model_validator(mode="after")
    def validate_species_definitions(self) -> "AutoignitionConditionConfig":  # noqa: C901
        if self.equivalence_ratio:
            amount_definition = (
                "Mole" if self.amount_definition_type == AmountDefinitionType.MOLES_FRACTIONS else "Mass"
            )

            if not (self.fuel and self.oxidizer):
                raise ValueError("No fuel or oxidizer specified")
            for fuel, mole_fraction in self.fuel.items():
                if mole_fraction < 0:
                    raise ValueError(f"{amount_definition} faction of fuel `{fuel}` is less than zero")
            for oxidizer, mole_fraction in self.oxidizer.items():
                if mole_fraction < 0:
                    raise ValueError(f"{amount_definition} faction of oxidizer `{oxidizer}` is less than zero")

            if self.reactants:
                raise ValueError(
                    "`Reactants` field can't be used with equivalence-ratio\\fuel\\oxidizer fields at the same time"
                )
            return self

        if not self.reactants:
            raise ValueError("No reactants or equivalence-ratio\\fuel\\oxidizer specified")

        for reactant, mass in self.reactants.items():
            if mass < 0:
                raise ValueError(f"Mass of reactant `{reactant}` is less than zero")

        return self

    def check_all_reactants_exist(self, species: set[str], ai_cond_idx: int) -> None:
        if self.reactants:
            for reactant in self.reactants:
                if reactant not in species:
                    raise ValueError(f"No such reactant `{reactant}` in model for {ai_cond_idx} case")
        if self.fuel:
            for fuel in self.fuel:
                if fuel not in species:
                    raise ValueError(f"No such fuel `{fuel}` in model for {ai_cond_idx} case")
        if self.oxidizer:
            for oxidizer in self.oxidizer:
                if oxidizer not in species:
                    raise ValueError(f"No such reactant `{oxidizer}` in model for {ai_cond_idx} case")


class ReducingTaskConfig(BaseModel):
    model_config = ConfigDict(BaseModel.model_config, alias_generator=alias_generator)

    model: Path
    phase_name: str | None = None
    target_species: list[str]
    retained_species: list[str] = Field(default_factory=list)
    method: ReducingMethod
    autoignition_conditions: list[AutoignitionConditionConfig] = Field(min_length=1)
    max_error: float = Field(lt=1.0, gt=0.0)

    initial_threshold: float = Field(default=0.01, gt=0)
    _initial_threshold_set_by_user: bool = False

    threshold_increment: float | None = Field(default=None, gt=0)

    initial_threshold_auto_retrieving_multiplier: float = Field(default=0.01, gt=0)
    initial_threshold_auto_retrieving_attempts: int = Field(default=3, gt=1)

    @model_validator(mode="before")
    @classmethod
    def checkinitial_threshold_set_by_user(cls, data: Any) -> Any:
        if isinstance(data, dict) and "initial-threshold" in data:
            data["_initial_threshold_set_by_user"] = True
        return data

    @model_validator(mode="before")
    @classmethod
    def recalc_max_error(cls, data: Any) -> Any:
        if isinstance(data, dict) and "max-error" in data:
            try:
                max_error = float(data["max-error"])
                data["max-error"] = max_error / 100
            except (ValueError, TypeError):
                # it will validated by pydantic
                pass
        return data

    def check_all_species_exist(self, species: set[str]) -> None:
        for specy in self.target_species + self.retained_species:
            if specy not in species:
                raise ValueError(f"No such specy `{specy}` in model")
        for i, ai_cond in enumerate(self.autoignition_conditions):
            ai_cond.check_all_reactants_exist(species, i)


class CliOptionsAdditionalInfo:
    def __init__(
        self, *, required: Literal[False] | None = None, count: bool = False, count_flag_char: str | None = None
    ) -> None:
        self.required = required
        if (
            count
            and count_flag_char is not None
            and (len(count_flag_char) != 1 or count_flag_char[0] not in ascii_lowercase)
        ):
            raise ValueError("Invalid count_flag_char argument")
        self.count = count
        self.count_flag_name = count_flag_char


class Config(BaseModel):
    input: Annotated[PathLike, Field(description="Path to an yaml file that describes reducing task")]
    output: Annotated[
        PathLike,
        Field(default=Path("./output-model.yaml"), description="Path to an yaml file that will store result model"),
    ]
    num_threads: Annotated[
        int,
        Field(
            ge=1,
            default=1,
            description="Count of cpu cores used to execute parallel simulations and reducing processes graphes. \
Default: n - 1 if system is mutlicores else 1",
        ),
    ]
    creating_matrixes_num_threads: Annotated[
        int,
        Field(
            ge=1,
            description="Count of processes that will be executed parallel to create matrixes. \
Default value equals --num-threads flag",
        ),
        CliOptionsAdditionalInfo(required=False),
    ]
    verbose: Annotated[
        int,
        Field(default=0, le=3, ge=0, description="Debug level. Can be repeated to increase level. Maximum is 3"),
        CliOptionsAdditionalInfo(count=True, count_flag_char="v"),
    ]

    no_colorized_logs: Annotated[bool, Field(default=False, description="Disable colorized output")]

    _reducing_task_config: ReducingTaskConfig | None = None
    _tmp_dir: PathLike | None = None

    @model_validator(mode="before")
    @classmethod
    def calc_num_threads(cls, data: Any) -> None:
        if isinstance(data, dict) and "num_threads" not in data:
            num_threads = cpu_count()
            if num_threads > 1:
                num_threads -= 1
            data["num_threads"] = num_threads
        if isinstance(data, dict) and "creating_matrixes_num_threads" not in data:
            data["creating_matrixes_num_threads"] = data["num_threads"]
        return data

    @property
    def tmp_dir(self) -> Path:
        if self._tmp_dir is None:
            raise ValueError("No value")
        return Path(self._tmp_dir)

    @property
    def reducing_task_config(self) -> ReducingTaskConfig:
        if self._reducing_task_config is None:
            raise ValueError("No value")
        return self._reducing_task_config
