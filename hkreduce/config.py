from pathlib import Path
from typing import Literal

from pydantic import BaseModel, Field


class AutoignitionCondition(BaseModel, validate_assignment=True):
    kind: Literal["CONSTANT_VOLUME", "CONSTANT_PRESSURE"]
    pressure: float
    temperature: float
    reactants: dict[str, float]

    max_steps: int = 10 * 1000
    end_of_time: float | None = None
    max_time_step: float | None = None
    residual_threshold_coef: float = 10.0


class ReducingTask(BaseModel, validate_assignment=True):
    model: Path
    phase_name: str | None = None
    target_species: list[str]
    retained_species: list[str] = Field(default_factory=list)
    method: Literal["DRG", "DRGEP", "PFA"]
    autoignition_conditions: list[AutoignitionCondition]
