from typing import Any

from setup import EXT_MODULES, CMakeBuilder


def build(setup_kwargs: dict[str, Any]) -> None:
    setup_kwargs.update(
        {
            "ext_modules": EXT_MODULES,
            "cmdclass": {"build_ext": CMakeBuilder},
            "zip_safe": False,
        }
    )
