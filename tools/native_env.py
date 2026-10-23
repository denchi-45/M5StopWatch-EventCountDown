# PlatformIO pre-script for the native (SDL2) simulator.
# On Windows, puts the MSYS2 UCRT64 toolchain (g++, SDL2.dll) first on PATH so
# that other MinGW copies earlier on PATH (e.g. Git for Windows' mingw64) do not
# shadow its DLLs. Override the location with the MSYS2_UCRT64_BIN variable.
import os
import sys

Import("env")  # noqa: F821 (provided by PlatformIO)

if sys.platform == "win32":
    ucrt = os.environ.get("MSYS2_UCRT64_BIN", r"C:\msys64\ucrt64\bin")
    if os.path.isfile(os.path.join(ucrt, "g++.exe")):
        env.PrependENVPath("PATH", ucrt)  # noqa: F821
        os.environ["PATH"] = ucrt + os.pathsep + os.environ.get("PATH", "")
