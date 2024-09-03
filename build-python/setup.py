import os
import subprocess
from pathlib import Path
from typing import Any, TypeAlias
import sys
import platform

from setuptools import Extension  # type: ignore[import-untyped]
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


class CMakeBuild(build_ext):
    @staticmethod
    def _get_env_variable(name: str, default='OFF'):
        if name not in os.environ:
            return default
        return os.environ[name]

    def build_extensions(self):
        try:
            subprocess.check_output(["cmake", "--version"]) # noqa: S607, S603
        except OSError as error:
            raise RuntimeError("Cannot find CMake executable") from error

        for ext in self.extensions:
            extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext_name=ext.name)))
            cfg = "Debug" if self._get_env_variable("DISPTOOLS_DEBUG") == "ON" else "Release"

            cmake_args = [
                "-DDISPTOOLS_DEBUG=%s" % ("ON" if cfg == "Debug" else "OFF"),
                "-DDISPTOOLS_OPT=%s" % self._get_env_variable("DISPTOOLS_OPT"),
                "-DDISPTOOLS_DOUBLE=%s" % self._get_env_variable("DISPTOOLS_DOUBLE"),
                "-DDISPTOOLS_PYTHON_C_MODULE_NAME=%s" % c_module_name,
                "-DCMAKE_BUILD_TYPE=%s" % cfg,
                "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{}={}".format(cfg.upper(), extdir),
                "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_{}={}".format(cfg.upper(), self.build_temp),
                "-DPYTHON_EXECUTABLE={}".format(sys.executable),
            ]

            if platform.system() == "Windows":
                plat = "x64" if platform.architecture()[0] == "64bit" else "Win32"
                cmake_args += [
                    "-DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=TRUE",
                    "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_{}={}".format(cfg.upper(), extdir),
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

            cmake_args += cmake_cmd_args

            pprint(cmake_args)

            if not os.path.exists(self.build_temp):
                os.makedirs(self.build_temp)

            # Config and build the extension
            subprocess.check_call(["cmake", ext.cmake_lists_dir] + cmake_args, cwd=self.build_temp)
            subprocess.check_call(["cmake", "--build", ".", "--config", cfg], cwd=self.build_temp)
