from pathlib import Path
from typing import Any, Literal, TypeAlias

PathLike: TypeAlias = Path | str

ReducingMethod: TypeAlias = Literal["DRG", "DRGEP", "PFA"]


LoggingConfig: TypeAlias = dict[str, Any]
