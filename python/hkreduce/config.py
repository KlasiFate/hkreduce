from multiprocessing import cpu_count
from pathlib import Path
from typing import Any, Literal

from pydantic import BaseModel as PydanticBaseModel
from pydantic import Field, model_validator

from .typing import ReducingMethod


class BaseModel(PydanticBaseModel, validate_assignment=True):
    pass


class AutoignitionConditionConfig(BaseModel):
    kind: Literal["CONSTANT_VOLUME", "CONSTANT_PRESSURE"]
    pressure: float
    temperature: float
    reactants: dict[str, float]

    max_steps: int = Field(default=10 * 1000, gt=0)
    end_of_time: float | None = Field(default=None, gt=0.0)
    max_time_step: float | None = Field(default=None, gt=0.0)
    residual_threshold_coef: float = Field(default=10.0, gt=0.0)
    steps_sample_size: int = Field(default=20, gt=0)

    @model_validator(mode='after')
    def validate_reactants(self) -> 'AutoignitionConditionConfig':
        for reactant, mass in self.reactants.items():
            if mass < 0:
                raise ValueError(f"Mass of reactant `{reactant}` is less than zero")
        return self

    def check_all_reactants_exist(self, species: set[str], ai_cond_idx: int) -> None:
        for reactant in self.reactants:
            if reactant not in species:
                raise ValueError(f"No such reactant `{reactant}` in model for {ai_cond_idx} case")

class ReducingTaskConfig(BaseModel):
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

    @model_validator(mode='before')
    @classmethod
    def checkinitial_threshold_set_by_user(cls, data: Any) -> Any:
        if isinstance(data, dict) and 'initial_threshold' in data:
            data['_initial_threshold_set_by_user'] = True
        return data

    def check_all_species_exist(self, species: set[str]) -> None:
        for specy in self.target_species + self.retained_species:
            raise ValueError(f"No such specy `{specy}` in model")
        for i, ai_cond in enumerate(self.autoignition_conditions):
            ai_cond.check_all_reactants_exist(species, i)


class Config(BaseModel):
    input: Path
    output: Path = Path("./output-model.yaml")
    num_threads: int = Field(ge=1)
    debug: bool = False

    @model_validator(mode="before")
    @classmethod
    def calc_num_threads(cls, data: Any) -> None:
        if isinstance(data, dict) and "num_threads" not in data:
            num_threads = cpu_count()
            if num_threads > 1:
                num_threads -= 1
            data["num_threads"] = num_threads
        return data
