# SPDX-License-Identifier: BSD-2-Clause
#
# Compiler unit tests. One executable holds every test case; doctest's own
# command line (e.g. --test-case="*-o*") selects individual cases when needed.

add_library(doctest INTERFACE)
# SYSTEM: warnings inside the vendored header are not ours to fix.
target_include_directories(doctest SYSTEM INTERFACE ${PROJECT_SOURCE_DIR}/third_party/doctest)

add_executable(tf_unit_tests
    main.cpp
    test_command_line.cpp)

target_link_libraries(tf_unit_tests PRIVATE tfc_lib doctest)
tf_set_warnings(tf_unit_tests)

add_test(NAME unit COMMAND tf_unit_tests)
set_tests_properties(unit PROPERTIES LABELS unit)
