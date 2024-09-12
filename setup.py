import os
import platform
import re
import subprocess
import sys
from pathlib import Path
from pprint import pprint
from typing import Any, Literal, TypeAlias

from setuptools import Extension, setup  # type: ignore[import-untyped]
from setuptools.command.build_ext import build_ext  # type: ignore[import-untyped]

PathLike: TypeAlias = str | Path


class CMakeExtension(Extension):
    def __init__(
        self, name: str, cmake_lists_dir: PathLike = ".", sources: list[PathLike] | None = None, **kwargs: Any
    ):
        if sources is None:
            sources = []
        super().__init__(name, sources=sources, **kwargs)
        self.cmake_lists_dir = os.path.abspath(cmake_lists_dir)


class CMakeBuilder(build_ext):
    def build_extensions(self):
        try:
            subprocess.check_output(["cmake", "--version"])  # noqa: S607, S603
        except OSError as error:
            raise RuntimeError("Cannot find CMake executable") from error

        for ext in self.extensions:
            extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext_name=ext.name)))
            cfg = "Debug" if self.debug == "ON" else "Release"

            cmake_args = [
                f"-DCMAKE_BUILD_TYPE={cfg}",
                f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{cfg.upper()}={extdir}",
                f"-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_{cfg.upper()}={self.build_temp}",
                f"-DPYTHON_EXECUTABLE={sys.executable}",
            ]

            if platform.system() == "Windows":
                plat = "x64" if platform.architecture()[0] == "64bit" else "Win32"
                cmake_args += [
                    "-DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=TRUE",
                    f"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_{cfg.upper()}={extdir}",
                ]
                if self.compiler.compiler_type == "msvc":
                    cmake_args += [
                        "-DCMAKE_GENERATOR_PLATFORM=%s" % plat,
                    ]
                else:
                    cmake_args += [
                        "-G",
                        "MinGW Makefiles",
                    ]

            pprint(cmake_args)  # noqa: T203

            if not os.path.exists(self.build_temp):
                os.makedirs(self.build_temp)

            # Config and build the extension
            subprocess.check_call(["cmake", ext.cmake_lists_dir] + cmake_args, cwd=self.build_temp)  # noqa: S603
            subprocess.check_call(["cmake", "--build", ".", "--config", cfg], cwd=self.build_temp)  # noqa: S603, S607


PROJECT_DIR = Path(__file__).parent
CPP_FILES_DIR = PROJECT_DIR / "cpp" / "src" / "hkreduce" / "python_interface"

EXT_MODULES = [
    CMakeExtension(
        name="hkreduce.cpp_interface",
        cmake_lists_dir=PROJECT_DIR,
        #  sources=[CPP_FILES_DIR / 'types.cpp', CPP_FILES_DIR / 'default_allocator.cpp']
    )
]


class SetuptoolsUsage:
    def __init__(self) -> None:
        self.parsed_pyproject = self._read_pyproject_toml()
        self.conf = self.parsed_pyproject["tool"]["poetry"]

    @classmethod
    def _read_pyproject_toml(cls) -> dict[str, Any]:
        import tomli

        with open(PROJECT_DIR / "pyproject.toml", "rb") as pyproject_file:
            return tomli.load(pyproject_file)

    def _split_nicknames_and_emails(self, who: Literal["authors", "maintainers"]) -> tuple[str, str]:
        pattern = re.compile(r"(?P<nickname>.+)\s+\<(?P<email>.+)\>", flags=re.IGNORECASE)
        nicknames: list[str] = []
        emails: list[str] = []
        source = self.conf.get("maintainers") if who == "maintainers" else self.conf.get("authors")
        if source is None:
            raise ValueError(f"No information about {who}")
        for author_or_maintainer in source:
            match = pattern.search(author_or_maintainer)
            if match is None:
                raise ValueError(f"String `{author_or_maintainer}` can't be parsed")
            nicknames.append(match.group("nickname"))
            emails.append(match.group("email"))
        return ", ".join(nicknames), ", ".join(emails)

    def _parse_packages(self) -> tuple[list[str], dict[str, str]]:
        names: list[str] = []
        packages_dirs: dict[str, str] = {}
        for package in self.conf["packages"]:
            name = package["include"]
            names.append(name)
            if "from" in package:
                packages_dirs[name] = str(PROJECT_DIR / Path(package["from"]))

        return names, packages_dirs

    def _get_options(self) -> dict[str, Any]:
        return {"zip_safe": False}

    def _get_dependencies(self) -> list[str]:
        # TODO: implement support of git's dependencies
        dependencies: list[str] = []
        for name, version_spec in self.conf["dependencies"]:
            if name == "python":
                continue
            dependencies.append(f"{name}{version_spec}")
        return dependencies

    def __call__(self) -> None:
        authors, authors_emails = self._split_nicknames_and_emails("authors")
        kwargs: dict[str, Any] = {}
        try:
            maintainers, maintainers_emails = self._split_nicknames_and_emails("maintainers")
            kwargs["maintainer"] = maintainers
            kwargs["maintainer_email"] = maintainers_emails
        except ValueError:
            pass

        packages, packages_dirs = self._parse_packages()
        if packages_dirs:
            kwargs["package_dir"] = packages_dirs

        setup(
            name=self.conf["name"],
            version=self.conf["version"],
            description=self.conf["description"],
            author=authors,
            email=authors_emails,
            # ---
            packages=packages,
            ext_modules=EXT_MODULES,
            cmdclass={"build_ext": CMakeBuilder},
            options=self._get_options(),
            install_requires=self._get_dependencies(),
            python_requires=self.conf["dependencies"]["python"],
            **kwargs,
        )


# When developing to use setup tools
if __name__ == "__main__":
    SetuptoolsUsage()()
