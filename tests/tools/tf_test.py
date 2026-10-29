# SPDX-License-Identifier: BSD-2-Clause
"""Runs one TermForms end-to-end test. Invoked by CTest (see tests/CMakeLists.txt).

Modes:

  golden  Compile NAME.bas with --emit-c and compare the C with NAME.c.
  errors  Compile NAME.bas and compare the diagnostics with NAME.err.
  run     Build NAME.bas into an executable, run it headless with the key
          script NAME.keys, and compare the final screen with NAME.screen.
          A NAME.files folder's contents are copied to where it runs
          (dated 2 Jan 2026 03:04:05, local time).
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
import time
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


def sources(source: Path) -> list:
    """The test's .bas, then any further modules its .with file lists (one
    path a line, relative to it): a program of several modules."""
    with_file = source.with_suffix(".with")
    more = with_file.read_text(encoding="utf-8").split() if with_file.exists() else []
    return [source.name] + more


def golden(tfc: str, source: Path) -> bool:
    result = run_tfc(tfc, ["--emit-c", "-o", "-"] + sources(source), source.parent)
    if result.returncode != 0:
        print(f"FAIL: tfc exited {result.returncode}\n{result.stderr}")
        return False
    return compare("generated C", source.with_suffix(".c"), result.stdout)


def errors(tfc: str, source: Path) -> bool:
    result = run_tfc(tfc, ["--emit-c", "-o", "-"] + sources(source), source.parent)
    expected_code = 2 if "error:" in result.stderr else 0
    if result.returncode != expected_code:
        print(f"FAIL: tfc exited {result.returncode}, expected {expected_code}\n{result.stderr}")
        return False
    return compare("diagnostics", source.with_suffix(".err"), result.stderr)


def run(tfc: str, source: Path, work: Path) -> bool:
    work.mkdir(parents=True, exist_ok=True)
    exe = work / (source.stem + (".exe" if os.name == "nt" else ""))
    build = run_tfc(tfc, ["-o", str(exe)] + sources(source), source.parent)
    if build.returncode != 0:
        print(f"FAIL: build exited {build.returncode}\n{build.stdout}{build.stderr}")
        return False

    # The key script; a console program (no forms) needs none.
    keys_file = source.with_suffix(".keys")
    keys = keys_file.read_text(encoding="utf-8").rstrip("\r\n") if keys_file.exists() else ""
    # An optional .size file ("80x24"): a screen for programs larger than
    # the usual one, such as the examples.
    size_file = source.with_suffix(".size")
    size = size_file.read_text(encoding="utf-8").strip() if size_file.exists() else HEADLESS_SIZE
    env = dict(os.environ, TERMFORMS_HEADLESS=keys, TERMFORMS_HEADLESS_SIZE=size)
    # Each program runs in a folder of its own, emptied first: files it
    # writes stay out of the source tree, and tests run in parallel safely.
    folder = work / (source.stem + ".dir")
    shutil.rmtree(folder, ignore_errors=True)
    # An optional NAME.files folder: files the program finds where it runs.
    given = source.parent / (source.stem + ".files")
    if given.is_dir():
        shutil.copytree(given, folder)
        # Dated 2 Jan 2026 03:04:05 local time, so FileDateTime shows the
        # same on every machine, whatever its time zone or checkout time.
        fixed = time.mktime((2026, 1, 2, 3, 4, 5, 0, 0, -1))
        for path in folder.rglob("*"):
            os.utime(path, (fixed, fixed))
    else:
        folder.mkdir()
    # An optional .args file: the program's command-line arguments, one a line.
    args_file = source.with_suffix(".args")
    args = args_file.read_text(encoding="utf-8").splitlines() if args_file.exists() else []
    # An optional .stdin file: the program's standard input (else none, so
    # a console program never waits on the terminal running the tests).
    stdin_file = source.with_suffix(".stdin")
    stdin = stdin_file.read_bytes() if stdin_file.exists() else b""
    result = subprocess.run([str(exe)] + args, cwd=folder, env=env, input=stdin,
                            capture_output=True, timeout=60)
    stdout = result.stdout.decode("utf-8", errors="replace")
    stderr = result.stderr.decode("utf-8", errors="replace")

    # An optional .exit file: the exit code expected (End n); else 1 with a
    # .stderr (a runtime error), 0 without.
    exit_file = source.with_suffix(".exit")
    stderr_file = source.with_suffix(".stderr")
    ok = True
    if stderr_file.exists() or os.environ.get("TF_UPDATE_EXPECTED") == "1" and stderr:
        expected = int(exit_file.read_text().strip()) if exit_file.exists() else 1
        if result.returncode != expected:
            print(f"FAIL: program exited {result.returncode}, expected {expected}")
            ok = False
        ok = compare("stderr", stderr_file, stderr) and ok
    else:
        expected = int(exit_file.read_text().strip()) if exit_file.exists() else 0
        if result.returncode != expected or stderr:
            print(f"FAIL: program exited {result.returncode}, expected {expected}\n{stderr}")
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
