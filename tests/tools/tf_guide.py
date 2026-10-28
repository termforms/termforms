#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Documentation test: builds every complete program in docs/GUIDE.md, so
the guide cannot drift from the language. A program is a ```vb block with
a form block or a Sub Main; the other blocks are fragments and are left
out.

    tf_guide.py --tfc PATH --work DIR
"""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GUIDE = ROOT / "docs" / "GUIDE.md"


def programs(text: str):
    """Yields (line, source) for each complete program in the guide."""
    for match in re.finditer(r"^```vb\n(.*?)^```", text, re.S | re.M):
        source = match.group(1)
        if "Begin Form" in source or re.search(r"^Sub Main\(", source, re.M):
            yield text.count("\n", 0, match.start()) + 2, source


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tfc", required=True)
    parser.add_argument("--work", required=True)
    args = parser.parse_args()

    work = Path(args.work)
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)

    failures = 0
    count = 0
    for line, source in programs(GUIDE.read_text(encoding="utf-8")):
        count += 1
        path = work / f"guide_{line}.bas"
        path.write_text(source, encoding="utf-8")
        result = subprocess.run(
            [args.tfc, "-o", str(work / f"guide_{line}"), str(path)],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            failures += 1
            print(f"{GUIDE}:{line}: the program here does not build:")
            print(result.stdout + result.stderr)
    if count == 0:
        print(f"{GUIDE}: no complete programs found")
        return 1
    print(f"{count - failures} of {count} programs in the guide build")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
