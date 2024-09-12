# ruff: noqa: T201, T203

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
        sources = [str(source) for source in sources]
        super().__init__(name, sources=sources, **kwargs)
        self.cmake_lists_dir = os.path.abspath(cmake_lists_dir)


class CMakeBuilder(build_ext):
    def _patch_stupid_cmake(self) -> None:
        path = Path(self.build_temp) / "docs/CMakeFiles/sphinx.dir/build.make"
        with open(path, "rt") as file:
            file_content = file.read()
        lines = file_content.splitlines()
        problem_line1 = lines.pop(74)
        problem_line2 = lines.pop(74)
        fixed_line = problem_line1 + " " + problem_line2.strip()
        lines.insert(74, fixed_line)
        file_content = "\n".join(lines)
        with open(path, "wt") as file:
            file.write(file_content)

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

            print(f"\n{'='*10}\nCmake args\n{'='*10}\n")
            pprint(cmake_args)

            if not os.path.exists(self.build_temp):
                os.makedirs(self.build_temp)

            # Config and build the extension
            print(f"\n{'='*15}\nConfigure cmake\n{'='*15}\n")
            subprocess.check_call(["cmake", ext.cmake_lists_dir] + cmake_args, cwd=self.build_temp)  # noqa: S603
            self._patch_stupid_cmake()
            print(f"\n{'='*9}\nRun cmake\n{'='*9}\n")
            subprocess.check_call(["cmake", "--build", ".", "--config", cfg], cwd=self.build_temp)  # noqa: S603, S607
            print(f"\n{'='*21}\nEnd of cmake building\n{'='*21}\n")


PROJECT_DIR = Path(__file__).parent
CPP_SOURCE = PROJECT_DIR / "python/cpp_extension/hkreduce/cpp_interface.cpp"

EXT_MODULES = [CMakeExtension(name="hkreduce.cpp_interface", cmake_lists_dir=PROJECT_DIR, sources=[CPP_SOURCE])]


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

    @classmethod
    def _convert_version_spec(cls, version_spec: str) -> str:
        if not version_spec.startswith("^"):
            return version_spec
        lower_versions = [int(version) for version in version_spec[1::].split(".")]
        lower_versions = lower_versions[:3:]

        main_version = 0
        if lower_versions[0] == 0 and len(lower_versions) > 1:
            main_version = 1
            if lower_versions[1] == 0 and len(lower_versions) > 2:
                main_version = 2

        higher_versions: list[int] = [0 for i in range(main_version)]
        higher_versions.append(lower_versions[main_version] + 1)
        for __ in range(main_version + 1, 3):
            higher_versions.append(0)  # noqa: PERF401

        lower_version_spec = ".".join(str(version) for version in lower_versions)
        higher_version_spec = ".".join(str(version) for version in higher_versions)
        return f">={lower_version_spec},<{higher_version_spec}"

    def _get_dependencies(self) -> list[str]:
        # TODO: implement support of git's dependencies
        dependencies: list[str] = []
        for name, version_spec in self.conf["dependencies"].items():
            if name == "python":
                continue
            if version_spec.startswith("^"):
                version_spec = self._convert_version_spec(version_spec)
            dependencies.append(f"{name}{version_spec}")
        return dependencies

    def __call__(self) -> None:
        authors, authors_emails = self._split_nicknames_and_emails("authors")
        kwargs: dict[str, Any] = {"zip_safe": False}
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
            author_email=authors_emails,
            # ---
            packages=packages,
            ext_modules=EXT_MODULES,
            cmdclass={"build_ext": CMakeBuilder},
            install_requires=self._get_dependencies(),
            python_requires=self._convert_version_spec(self.conf["dependencies"]["python"]),
            **kwargs,
        )


# When developing to use setup tools
if __name__ == "__main__":
    SetuptoolsUsage()()
