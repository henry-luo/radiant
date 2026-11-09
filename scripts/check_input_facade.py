#!/usr/bin/env python3
"""Reject directory links and source files in Radiant's optional input-header facade."""

from __future__ import annotations

import os
from pathlib import Path
import posixpath
import re
import sys


ROOT = Path(__file__).resolve().parent.parent
HEADERS = {".h", ".hh", ".hpp", ".hxx", ".inc"}
SOURCES = {".c", ".cc", ".cpp", ".cxx", ".m", ".mm"}
INCLUDE = re.compile(r'^\s*#\s*(?:include|import)\s*["<]([^">]+)[">]', re.M)


def check_facade(root: Path) -> list[str]:
    violations = []
    for name in ("lib", "lambda"):
        facade = root / name
        if facade.is_symlink():
            violations.append(f"{facade}: directory-wide link")
            continue
        if not facade.exists():
            continue
        if not facade.is_dir():
            violations.append(f"{facade}: facade is not a directory")
            continue
        for current, directories, files in os.walk(facade, followlinks=False):
            for entry in directories:
                candidate = Path(current) / entry
                if candidate.is_symlink():
                    violations.append(f"{candidate}: directory-wide link")
            for entry in files:
                candidate = Path(current) / entry
                if candidate.suffix.lower() in SOURCES:
                    violations.append(f"{candidate}: source file in header facade")
                elif candidate.is_symlink() and (
                    candidate.suffix.lower() not in HEADERS or
                    not candidate.is_file() or candidate.resolve().suffix.lower() not in HEADERS
                ):
                    violations.append(f"{candidate}: link is not an existing header")
    # Header search paths must not be used to compile input sources by inclusion.
    for name in ("radiant", "test"):
        for source in (root / name).rglob("*"):
            if not source.is_file() or source.suffix.lower() not in SOURCES | HEADERS:
                continue
            for include in INCLUDE.findall(source.read_text(encoding="utf-8", errors="replace")):
                resolved = posixpath.normpath(posixpath.join(
                    source.relative_to(root).parent.as_posix(), include)) if include.startswith(".") else include
                if resolved.startswith(("lib/", "lambda/")) and Path(resolved).suffix.lower() in SOURCES:
                    violations.append(f"{source}: input source include {include}")
    return violations


def main() -> int:
    violations = check_facade(ROOT)
    if violations:
        for violation in violations:
            print(f"RADIANT_INPUT_FACADE: {violation}", file=sys.stderr)
        return 1
    print("RADIANT_INPUT_FACADE: no directory links or input source files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
