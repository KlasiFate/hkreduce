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

    max_steps: int = 10 * 1000
    end_of_time: float | None = None
    max_time_step: float | None = None
    residual_threshold_coef: float = 10.0
    steps_sample_size: int = 20


class ReducingTaskConfig(BaseModel):
    model: Path
    phase_name: str | None = None
    target_species: list[str]
    retained_species: list[str] = Field(default_factory=list)
    method: ReducingMethod
    autoignition_conditions: list[AutoignitionConditionConfig]
    max_error: float

    initial_threshold: float = 0.01
    _initial_threshold_set_by_user: bool = False

    threshold_increment: float | None = None

    initial_threshold_auto_retrieving_multiplier: float = 0.01
    initial_threshold_auto_retrieving_attempts: int = 3
    


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
