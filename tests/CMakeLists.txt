# SPDX-License-Identifier: BSD-2-Clause
#
# Test suites. All are registered with CTest, so "ctest --preset <name>" runs
# everything. Labels let a subset be run on its own, e.g. "ctest -L unit".
#
#   unit/     C++ unit tests for the compiler, using doctest (vendored)
#   runtime/  C tests for the runtime library
#   cli       command-line behaviour of the tfc executable (defined below)
#
# Golden tests (.bas -> expected C) and run tests (.bas -> expected output)
# arrive in Phase 2, once the compiler produces C.

add_subdirectory(unit)
add_subdirectory(runtime)

# ---------------------------------------------------------------------------
# Command-line tests: run tfc and match its output. When
# PASS_REGULAR_EXPRESSION is set, CTest judges by output alone, so the
# failing-exit cases are checked by their messages.
# ---------------------------------------------------------------------------
add_test(NAME cli.version COMMAND tfc --version)
set_tests_properties(cli.version PROPERTIES
    LABELS cli
    PASS_REGULAR_EXPRESSION "^tfc \\(TermForms\\) ${PROJECT_VERSION}")

add_test(NAME cli.help COMMAND tfc --help)
set_tests_properties(cli.help PROPERTIES
    LABELS cli
    PASS_REGULAR_EXPRESSION "Usage: tfc")

add_test(NAME cli.no-inputs COMMAND tfc)
set_tests_properties(cli.no-inputs PROPERTIES
    LABELS cli
    PASS_REGULAR_EXPRESSION "no input files")

add_test(NAME cli.unknown-option COMMAND tfc --frobnicate)
set_tests_properties(cli.unknown-option PROPERTIES
    LABELS cli
    PASS_REGULAR_EXPRESSION "unknown option '--frobnicate'")

# Exit-code checks: these must fail (non-zero exit) to pass.
add_test(NAME cli.no-inputs-exit COMMAND tfc)
set_tests_properties(cli.no-inputs-exit PROPERTIES LABELS cli WILL_FAIL TRUE)

add_test(NAME cli.not-implemented-exit COMMAND tfc hello.bas)
set_tests_properties(cli.not-implemented-exit PROPERTIES LABELS cli WILL_FAIL TRUE)
