# SPDX-License-Identifier: 0BSD
#
# Runtime tests are plain C programs: exit status 0 means pass. They link the
# runtime exactly as a compiled TermForms program will.

add_executable(test_runtime_version test_runtime_version.c)
target_link_libraries(test_runtime_version PRIVATE tfrt)
target_compile_definitions(test_runtime_version PRIVATE TF_EXPECTED_VERSION="${PROJECT_VERSION}")
tf_set_warnings(test_runtime_version)

add_test(NAME runtime.version COMMAND test_runtime_version)
set_tests_properties(runtime.version PROPERTIES LABELS runtime)

add_executable(test_numbers test_numbers.c)
target_link_libraries(test_numbers PRIVATE tfrt)
tf_set_warnings(test_numbers)

add_test(NAME runtime.numbers COMMAND test_numbers)
set_tests_properties(runtime.numbers PROPERTIES LABELS runtime)
