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

add_executable(test_errors test_errors.c)
target_link_libraries(test_errors PRIVATE tfrt)
tf_set_warnings(test_errors)

add_test(NAME runtime.errors COMMAND test_errors)
set_tests_properties(runtime.errors PROPERTIES LABELS runtime)

add_executable(test_arrays test_arrays.c)
target_link_libraries(test_arrays PRIVATE tfrt)
tf_set_warnings(test_arrays)

add_test(NAME runtime.arrays COMMAND test_arrays)
set_tests_properties(runtime.arrays PROPERTIES LABELS runtime)

add_executable(test_dict test_dict.c)
target_link_libraries(test_dict PRIVATE tfrt)
tf_set_warnings(test_dict)

add_test(NAME runtime.dict COMMAND test_dict)
set_tests_properties(runtime.dict PROPERTIES LABELS runtime)

add_executable(test_text test_text.c)
target_link_libraries(test_text PRIVATE tfrt)
tf_set_warnings(test_text)

add_test(NAME runtime.text COMMAND test_text)
set_tests_properties(runtime.text PROPERTIES LABELS runtime)

add_executable(test_keys test_keys.c)
target_link_libraries(test_keys PRIVATE tfrt)
tf_set_warnings(test_keys)

add_test(NAME runtime.keys COMMAND test_keys)
set_tests_properties(runtime.keys PROPERTIES LABELS runtime)
