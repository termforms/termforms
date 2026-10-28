#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Debugger test: builds tests/debug/sample.bas, runs it under lldb or gdb
with TermForms' printers (debug/), and checks that a breakpoint on a .bas
line stops there, that Strings, arrays, a Dictionary and Refs show as
BASIC would, and that stepping goes into a BASIC Function, not into the
runtime. Exits 77 (skipped) when the debugger is not installed.

    tf_debug.py --tfc PATH --debugger lldb|gdb --work DIR
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SAMPLE = ROOT / "tests" / "debug" / "sample.bas"
SKIP = 77


def stop_line() -> int:
    for number, text in enumerate(SAMPLE.read_text(encoding="utf-8").splitlines(), 1):
        if "' STOP" in text:
            return number
    raise SystemExit("no STOP line in " + str(SAMPLE))


def function_line() -> int:
    for number, text in enumerate(SAMPLE.read_text(encoding="utf-8").splitlines(), 1):
        if text.startswith("Function Twice"):
            return number
    raise SystemExit("no Function Twice in " + str(SAMPLE))


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tfc", required=True)
    parser.add_argument("--debugger", required=True, choices=["lldb", "gdb"])
    parser.add_argument("--work", required=True, type=Path)
    args = parser.parse_args()

    if os.name == "nt":
        # Windows debugging (Visual Studio, .natvis) is a question of its own.
        print("SKIP: not on Windows")
        return SKIP
    debugger = shutil.which(args.debugger)
    if debugger is None:
        print(f"SKIP: {args.debugger} is not installed")
        return SKIP

    work = args.work.resolve() / ("debug-" + args.debugger)
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    shutil.copy(SAMPLE, work / "sample.bas")
    exe = work / "sample"
    tfc = str(Path(args.tfc).resolve()) if os.sep in args.tfc or "/" in args.tfc else args.tfc
    build = subprocess.run([tfc, "-o", str(exe), "sample.bas"], cwd=work,
                           capture_output=True, text=True)
    if build.returncode != 0:
        print("FAIL: build\n" + build.stdout + build.stderr)
        return 1

    line = stop_line()
    env = dict(os.environ, TERMFORMS_HEADLESS="")
    if args.debugger == "lldb":
        script = ROOT / "debug" / "lldb" / "termforms.py"
        command = [debugger, "-b",
                   "-o", f"command script import {script}",
                   "-o", f"breakpoint set -f sample.bas -l {line}",
                   "-o", "run",
                   "-o", "frame variable v_s v_a v_g v_p v_d",
                   "-o", "frame variable -T v_refs",
                   "-o", "step",
                   "-o", "frame info",
                   "-o", "kill",
                   "--", str(exe)]
    else:
        script = ROOT / "debug" / "gdb" / "termforms.py"
        command = [debugger, "-batch", "-nx",
                   "-ex", f"source {script}",
                   "-ex", f"break sample.bas:{line}",
                   "-ex", "run",
                   "-ex", "print v_s", "-ex", "print v_a", "-ex", "print v_g",
                   "-ex", "print v_p", "-ex", "print v_d", "-ex", "print *v_refs.data",
                   "-ex", "print v_refs",
                   "-ex", "step", "-ex", "frame",
                   "-ex", "kill",
                   "--args", str(exe)]
    result = subprocess.run(command, cwd=work, env=env, capture_output=True, timeout=120)
    output = result.stdout.decode("utf-8", errors="replace") + \
        result.stderr.decode("utf-8", errors="replace")

    expected = [
        f"sample.bas:{line}",          # stopped at the BASIC line
        '"héllo"',                      # a String as its text
        "[2] = 3",                      # an array's elements
        "[1,1] = 4",                    # two dimensions, by subscripts
        '"pt1"',                        # a String inside a Type
        '["k1"] = 5',                   # a Dictionary by key
        "t_Point",                      # an array of Refs knows its Type
        f"sample.bas:{function_line()}",  # stepped into Twice, past tf_str_len
    ]
    missing = [e for e in expected if e not in output]
    # After the step, the session must be in Twice, not in the runtime's
    # tf_str_len (which Len calls first).
    after_step = output[output.rfind("step"):]
    if "tf_str_len" in after_step:
        missing.append("(the step went into the runtime: tf_str_len)")
    if missing:
        print("FAIL: not in the " + args.debugger + " session: " + ", ".join(missing))
        print(output)
        return 1
    print("ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
