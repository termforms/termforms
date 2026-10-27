# SPDX-License-Identifier: BSD-2-Clause
#
# Test suites. All are registered with CTest, so "ctest --preset <name>" runs
# everything. Labels let a subset be run on its own, e.g. "ctest -L unit".
#
#   unit/     C++ unit tests for the compiler, using doctest (vendored)
#   runtime/  C tests for the runtime library
#   cli       command-line behaviour of the tfc executable (defined below)
#
#   golden/   .bas -> expected generated C           (tools/tf_test.py golden)
#   errors/   .bas -> expected diagnostics           (tools/tf_test.py errors)
#   run/      .bas + key script -> expected screen   (tools/tf_test.py run)
#
# Adding a case is adding files: the directories are scanned at configure
# time (re-run CMake after adding one). TF_UPDATE_EXPECTED=1 rewrites the
# expected files from actual output.

add_subdirectory(unit)
add_subdirectory(runtime)

find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
set(TF_TEST_TOOL ${CMAKE_CURRENT_SOURCE_DIR}/tools/tf_test.py)

foreach(mode golden errors run)
    file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${mode}/*.bas)
    foreach(source ${sources})
        get_filename_component(name ${source} NAME_WE)
        add_test(NAME ${mode}.${name}
            COMMAND ${Python3_EXECUTABLE} ${TF_TEST_TOOL} ${mode}
                --tfc $<TARGET_FILE:tfc>
                --source ${source}
                --work ${CMAKE_CURRENT_BINARY_DIR}/run-work)
        set_tests_properties(${mode}.${name} PROPERTIES LABELS ${mode})
    endforeach()
endforeach()

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

add_test(NAME cli.missing-file-exit COMMAND tfc no-such-file.bas)
set_tests_properties(cli.missing-file-exit PROPERTIES LABELS cli WILL_FAIL TRUE)

# A missing C compiler, and one that runs but fails: tfc explains each in the
# programmer's terms instead of passing on C compiler output. Python, which
# ignores the arguments after -c's script, stands in for a broken compiler.
set(TF_CLI_WORK ${CMAKE_CURRENT_BINARY_DIR}/cli-work)
file(MAKE_DIRECTORY ${TF_CLI_WORK})
add_test(NAME cli.no-compiler
    COMMAND tfc --cc tf-no-such-compiler -o ${TF_CLI_WORK}/none
        ${PROJECT_SOURCE_DIR}/examples/hello.bas)
set_tests_properties(cli.no-compiler PROPERTIES
    LABELS cli
    PASS_REGULAR_EXPRESSION "cannot run the C compiler 'tf-no-such-compiler'; is it installed")

add_test(NAME cli.compiler-fails
    COMMAND tfc --cc "\"${Python3_EXECUTABLE}\" -c \"raise SystemExit(1)\"" -o ${TF_CLI_WORK}/broken
        ${PROJECT_SOURCE_DIR}/examples/hello.bas)
set_tests_properties(cli.compiler-fails PROPERTIES
    LABELS cli
    PASS_REGULAR_EXPRESSION "internal error: .*not in your program")
