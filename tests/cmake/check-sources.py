#!/usr/bin/env python3
"""Fails when the CMake build and meson.build list different files.

meson.build is the reference build; the CMake one (CMakeLists.txt, cmake/) repeats its lists.
Run from the repository root: python3 tests/cmake/check-sources.py
"""
import re
import sys
from pathlib import Path


def quoted(text: str, pattern: str) -> set[str]:
    return set(re.findall(rf"'({pattern})'", text))


def unquoted(text: str, pattern: str) -> set[str]:
    return set(re.findall(rf"(?<![\w/]){pattern}", text))


def moc_block(meson: str) -> str:
    start = meson.index("neoqcp_moc_headers = [")
    return meson[start:meson.index("]", start)]


def pairs() -> list[tuple[str, set[str], set[str]]]:
    meson = Path("meson.build").read_text()
    cmake = Path("cmake/sources.cmake").read_text() + Path("CMakeLists.txt").read_text()
    jkqt_meson = Path("subprojects/packagefiles/jkqtmathtext/meson.build").read_text()
    jkqt_cmake = Path("cmake/jkqtmathtext.cmake").read_text()
    sources, shaders = r"src/[\w/.-]+\.cpp", r"src/painting/shaders/[\w.-]+\.(?:vert|frag)"
    header = r"src/[\w/.-]+\.h"
    moc_cmake = cmake[cmake.index("set(NEOQCP_MOC_HEADERS"):]
    moc_cmake = moc_cmake[:moc_cmake.index(")")]
    return [
        ("sources", quoted(meson, sources), unquoted(cmake, sources)),
        ("shaders", quoted(meson, shaders), unquoted(cmake, shaders)),
        ("moc headers", quoted(moc_block(meson), header), unquoted(moc_cmake, header)),
        ("JKQTMathText files", quoted(jkqt_meson, r"lib/[\w/.-]+"),
         {"lib/" + f for f in re.findall(r"\$\{JKQT_LIB\}/([\w/.-]+)", jkqt_cmake)}),
    ]


def main() -> int:
    drift = False
    for name, in_meson, in_cmake in pairs():
        for path in sorted(in_meson - in_cmake):
            print(f"{name}: {path} is in the Meson build, not in the CMake one")
            drift = True
        for path in sorted(in_cmake - in_meson):
            print(f"{name}: {path} is in the CMake build, not in the Meson one")
            drift = True
    print("the CMake and Meson builds list different files" if drift else "the file lists match")
    return 1 if drift else 0


if __name__ == "__main__":
    sys.exit(main())
