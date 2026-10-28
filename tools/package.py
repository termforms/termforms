#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Packs a TermForms release for one platform:

    termforms-<version>-<platform>/
        bin/tfc[.exe]       the compiler
        runtime/            its sources and headers (tfc compiles them with
                            every program, for whatever target)
        debug/              lldb and gdb printers
        editors/vscode/     the VS Code extension and templates
        examples/           the example programs
        docs/               the guide, the reference, what works today
        README.md, LICENSE  (runtime/LICENSE and THIRD_PARTY_NOTICES inside)

as a .tar.gz (macOS, Linux) or .zip (Windows). Only files git tracks go
in, so nothing built or left over in the tree is shipped.

    package.py --tfc build/rel/compiler/tfc --platform macos-arm64 --out dist
"""

import argparse
import re
import shutil
import stat
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLATFORMS = ("macos-arm64", "linux-x86_64", "windows-x64")
TRACKED = ("runtime", "debug", "editors/vscode", "examples")
DOCS = ("GUIDE.md", "LANGUAGE.md", "REFERENCE.md", "LICENSING.md")


def version() -> str:
    text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    found = re.search(r"project\(\s*TermForms\s+VERSION\s+([0-9.]+)", text)
    if not found:
        raise SystemExit("no project(TermForms VERSION ...) in CMakeLists.txt")
    return found.group(1)


def tracked(folder: str) -> list:
    out = subprocess.run(["git", "ls-files", "-z", folder], cwd=ROOT, check=True,
                         capture_output=True).stdout.decode("utf-8")
    return [name for name in out.split("\0") if name]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tfc", required=True, type=Path)
    parser.add_argument("--platform", required=True, choices=PLATFORMS)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    name = f"termforms-{version()}-{args.platform}"
    out = args.out.resolve()
    stage = out / name
    shutil.rmtree(stage, ignore_errors=True)
    (stage / "bin").mkdir(parents=True)

    windows = args.platform.startswith("windows")
    tfc = stage / "bin" / ("tfc.exe" if windows else "tfc")
    shutil.copy2(args.tfc, tfc)
    tfc.chmod(tfc.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    for folder in TRACKED:
        for relative in tracked(folder):
            # The runtime's own build and tests are the repository's, not
            # a user's: tfc needs only include/ and src/.
            if relative == "runtime/CMakeLists.txt":
                continue
            target = stage / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, target)
    (stage / "docs").mkdir()
    for doc in DOCS:
        if (ROOT / "docs" / doc).exists():
            shutil.copy2(ROOT / "docs" / doc, stage / "docs" / doc)
    shutil.copy2(ROOT / "LICENSE", stage / "LICENSE")
    shutil.copy2(ROOT / "tools" / "release" / "README.md", stage / "README.md")

    archive = shutil.make_archive(str(out / name), "zip" if windows else "gztar",
                                  root_dir=out, base_dir=name)
    print(archive)
    return 0


if __name__ == "__main__":
    sys.exit(main())
