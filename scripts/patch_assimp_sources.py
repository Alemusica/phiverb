#!/usr/bin/env python3

import sys
from pathlib import Path


def patch_d3mf(source_root: Path) -> None:
    path = source_root / "code" / "D3MFImporter.cpp"
    if not path.exists():
        return
    text = path.read_text()
    needle = "        vertex.z = ai_strtof>(xmlReader->getAttributeValue(D3MF::XmlTag::z.c_str()), nullptr);"
    replacement = "        vertex.z = ai_strtof(xmlReader->getAttributeValue(D3MF::XmlTag::z.c_str()), nullptr);"
    if needle in text:
        path.write_text(text.replace(needle, replacement, 1))


def patch_zutil(source_root: Path) -> None:
    path = source_root / "contrib" / "zlib" / "zutil.h"
    if not path.exists():
        return
    text = path.read_text()
    needle = "#      ifndef fdopen"
    replacement = "#      if !defined(__APPLE__) && !defined(fdopen)"
    if needle in text:
        path.write_text(text.replace(needle, replacement, 1))


def patch_cmakelists(source_root: Path) -> None:
    path = source_root / "contrib" / "zlib" / "CMakeLists.txt"
    if not path.exists():
        return
    text = path.read_text()
    old_block = """# CMake 3.0 changed the project command, setting policy CMP0048 reverts to the old behaviour.
# See http://www.cmake.org/cmake/help/v3.0/policy/CMP0048.html
cmake_policy(PUSH)
if(CMAKE_MAJOR_VERSION GREATER 2)
\tcmake_policy(SET CMP0048 OLD)
endif()
project(zlib C)
cmake_policy(POP)
"""
    new_block = """# CMake 3.0 changed the project command, setting policy CMP0048 reverts to the old behaviour.
# See http://www.cmake.org/cmake/help/v3.0/policy/CMP0048.html
cmake_policy(PUSH)
if(CMAKE_MAJOR_VERSION GREATER 2)
\tif(POLICY CMP0048)
\t\tcmake_policy(SET CMP0048 NEW)
\tendif()
endif()
project(zlib C)
cmake_policy(POP)
"""
    if old_block in text:
        path.write_text(text.replace(old_block, new_block, 1))


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("Usage: patch_assimp_sources.py <assimp_source>")
    source_root = Path(sys.argv[1])
    patch_d3mf(source_root)
    patch_zutil(source_root)
    patch_cmakelists(source_root)


if __name__ == "__main__":
    main()
