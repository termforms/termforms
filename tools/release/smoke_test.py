#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Checks a packed release as a user would get it: unpacks the archive in
a fresh folder, then with its own bin/tfc (and the runtime beside it)
builds examples/hello.bas and runs it headless. The C compiler is tfc's
usual choice, or $TFC_CC.

    smoke_test.py dist/termforms-0.9.0-linux-x86_64.tar.gz
"""

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    archive = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as scratch:
        shutil.unpack_archive(str(archive), scratch)
        root = next(Path(scratch).iterdir())
        windows = os.name == "nt"
        tfc = root / "bin" / ("tfc.exe" if windows else "tfc")
        if not windows:
            tfc.chmod(0o755)  # zip and some unpackers drop the mode
        examples = root / "examples"
        print(subprocess.run([str(tfc), "--version"], check=True, capture_output=True,
                             text=True).stdout.strip())
        # -v shows the C compiler's command: it must use this release's runtime.
        build = subprocess.run([str(tfc), "-v", "hello.bas"], cwd=examples, capture_output=True,
                               text=True)
        print(build.stdout + build.stderr)
        if build.returncode != 0:
            print("FAIL: tfc could not build hello.bas")
            return 1
        if str(root / "runtime") not in build.stdout + build.stderr:
            print("FAIL: tfc did not use the runtime beside it")
            return 1
        hello = examples / ("hello.exe" if windows else "hello")
        env = dict(os.environ, TERMFORMS_HEADLESS="Jim{TAB}{ENTER}")
        run = subprocess.run([str(hello)], cwd=examples, env=env, capture_output=True)
        screen = run.stdout.decode("utf-8", errors="replace")
        if run.returncode != 0 or "Hello, Jim" not in screen:
            print("FAIL: hello did not greet\n" + screen + run.stderr.decode("utf-8", "replace"))
            return 1
        print("ok: " + archive.name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
