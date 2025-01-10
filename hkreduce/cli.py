from typing import Any, Callable, Optional, get_args

import click
from pydantic_core import PydanticUndefined

from .config import CliOptionsAdditionalInfo, Config

# supported types for cli options and their priority
PRIMITIVE_TYPES = [
    bool,
    int,
    str,
]


def gen_options(main_func: Callable) -> None:  # noqa: C901
    for field_name, field_info in reversed(list(Config.model_fields.items())):
        if field_name.startswith("_"):
            continue

        click_kwargs: dict[str, Any] = {}

        cli_option_info: CliOptionsAdditionalInfo | None = None
        if field_info.metadata:
            for meta_data_element in field_info.metadata:
                if isinstance(meta_data_element, CliOptionsAdditionalInfo):
                    cli_option_info = meta_data_element

        types: list[type[Any]] = []
        if isinstance(field_info.annotation, (type(str | int), type(Optional[int]))):
            types.extend(get_args(field_info.annotation))
        else:
            types.append(field_info.annotation)
        types = list(set(types).intersection(PRIMITIVE_TYPES))
        if not types:
            raise TypeError(f'No types can be used for cli option. Field name: "{field_name}"')
        types.sort(key=lambda x: PRIMITIVE_TYPES.index(x))
        option_type = types[0]

        if option_type is bool:
            click_kwargs["is_flag"] = True
            if field_info.default is PydanticUndefined:
                raise ValueError(f'Field "{field_name}" is flag option and it doesn\'t have default')
            if field_info.default:
                raise ValueError(f'Field "{field_name}" is flag option and it has default value True')

        click_kwargs["required"] = field_info.is_required()
        if field_info.is_required() and cli_option_info and cli_option_info.required is not None:
            click_kwargs["required"] = cli_option_info.required

        if field_info.default is not PydanticUndefined:
            click_kwargs["default"] = field_info.default

        if field_info.description:
            click_kwargs["help"] = field_info.description

        option_decls = []
        if len(field_name) == 1:
            option_decls.append(f"-{field_name}")
        else:
            option_decls.append(f"--{field_name.replace('_', '-')}")

        if option_type is int and cli_option_info and cli_option_info.count:
            click_kwargs["count"] = True
            if cli_option_info.count_flag_name:
                ad_decl = f"-{cli_option_info.count_flag_name}"
                if ad_decl not in option_decls:
                    option_decls.append(ad_decl)

        main_func = click.option(*option_decls, **click_kwargs)(main_func)

    return click.command(main_func)
