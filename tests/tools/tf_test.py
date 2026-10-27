# SPDX-License-Identifier: BSD-2-Clause
"""Runs one TermForms end-to-end test. Invoked by CTest (see tests/CMakeLists.txt).

Modes:

  golden  Compile NAME.bas with --emit-c and compare the C with NAME.c.
  errors  Compile NAME.bas and compare the diagnostics with NAME.err.
  run     Build NAME.bas into an executable, run it headless with the key
          script NAME.keys, and compare the final screen with NAME.screen.
          If NAME.stderr exists the program must fail (exit 1) with that
          message on stderr; otherwise it must exit 0 with nothing on stderr.

Every mode runs tfc in the test's own directory with a bare file name, so
file names in #line directives and messages are the same on every machine.

Set TF_UPDATE_EXPECTED=1 to rewrite the expected files from the actual
output instead of comparing (review the diff before committing).

The C compiler for run tests is tfc's default, or $TFC_CC when set.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

HEADLESS_SIZE = "60x14"


def normalise(text: str) -> str:
    """Compares text independent of line endings and trailing blank lines."""
    return text.replace("\r\n", "\n").rstrip("\n") + "\n"


def compare(label: str, expected_path: Path, actual: str) -> bool:
    if os.environ.get("TF_UPDATE_EXPECTED") == "1":
        expected_path.write_text(normalise(actual), encoding="utf-8", newline="\n")
        print(f"updated {expected_path.name}")
        return True
    if not expected_path.exists():
        print(f"FAIL: {expected_path.name} is missing; actual {label}:\n{actual}")
        return False
    expected = normalise(expected_path.read_text(encoding="utf-8"))
    if normalise(actual) == expected:
        return True
    print(f"FAIL: {label} differs from {expected_path.name}")
    print("--- expected ---")
    print(expected, end="")
    print("--- actual ---")
    print(normalise(actual), end="")
    return False


def run_tfc(tfc: str, args: list, cwd: Path) -> subprocess.CompletedProcess:
    return subprocess.run([tfc] + args, cwd=cwd, capture_output=True, text=True,
                          encoding="utf-8", errors="replace")


def golden(tfc: str, source: Path) -> bool:
    result = run_tfc(tfc, ["--emit-c", "-o", "-", source.name], source.parent)
    if result.returncode != 0:
        print(f"FAIL: tfc exited {result.returncode}\n{result.stderr}")
        return False
    return compare("generated C", source.with_suffix(".c"), result.stdout)


def errors(tfc: str, source: Path) -> bool:
    result = run_tfc(tfc, ["--emit-c", "-o", "-", source.name], source.parent)
    expected_code = 2 if "error:" in result.stderr else 0
    if result.returncode != expected_code:
        print(f"FAIL: tfc exited {result.returncode}, expected {expected_code}\n{result.stderr}")
        return False
    return compare("diagnostics", source.with_suffix(".err"), result.stderr)


def run(tfc: str, source: Path, work: Path) -> bool:
    work.mkdir(parents=True, exist_ok=True)
    exe = work / (source.stem + (".exe" if os.name == "nt" else ""))
    build = run_tfc(tfc, ["-o", str(exe), source.name], source.parent)
    if build.returncode != 0:
        print(f"FAIL: build exited {build.returncode}\n{build.stdout}{build.stderr}")
        return False

    keys = source.with_suffix(".keys").read_text(encoding="utf-8").rstrip("\r\n")
    env = dict(os.environ, TERMFORMS_HEADLESS=keys, TERMFORMS_HEADLESS_SIZE=HEADLESS_SIZE)
    # Each program runs in a folder of its own, emptied first: files it
    # writes stay out of the source tree, and tests run in parallel safely.
    folder = work / (source.stem + ".dir")
    shutil.rmtree(folder, ignore_errors=True)
    folder.mkdir()
    result = subprocess.run([str(exe)], cwd=folder, env=env, capture_output=True,
                            timeout=60)
    stdout = result.stdout.decode("utf-8", errors="replace")
    stderr = result.stderr.decode("utf-8", errors="replace")

    stderr_file = source.with_suffix(".stderr")
    ok = True
    if stderr_file.exists() or os.environ.get("TF_UPDATE_EXPECTED") == "1" and stderr:
        if result.returncode != 1:
            print(f"FAIL: program exited {result.returncode}, expected 1")
            ok = False
        ok = compare("stderr", stderr_file, stderr) and ok
    elif result.returncode != 0 or stderr:
        print(f"FAIL: program exited {result.returncode}\n{stderr}")
        ok = False
    return compare("screen", source.with_suffix(".screen"), stdout) and ok


def main() -> int:
    # Expected screens contain box-drawing characters; a Windows console
    # code page cannot print them otherwise.
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["golden", "errors", "run"])
    parser.add_argument("--tfc", required=True)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--work", type=Path, help="directory for built programs (run mode)")
    args = parser.parse_args()

    source = args.source.resolve()
    if args.mode == "golden":
        ok = golden(args.tfc, source)
    elif args.mode == "errors":
        ok = errors(args.tfc, source)
    else:
        ok = run(args.tfc, source, args.work.resolve())
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
