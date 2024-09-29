from enum import Enum
from pathlib import Path
from typing import Any, TypeAlias

PathLike: TypeAlias = Path | str


class ReducingMethod(str, Enum):
    DRG = "DRG"
    DRGEP = "DRGEP"
    PFA = "PFA"


LoggingConfig: TypeAlias = dict[str, Any]


class AmountDefinitionType(str, Enum):
    MOLES_FRACTIONS = "MOLES_FRACTIONS"
    MASS_FRACTIONS = "MASS_FRACTIONS"
