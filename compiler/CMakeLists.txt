# SPDX-License-Identifier: BSD-2-Clause
#
# The TermForms compiler and build driver.
#
#   tfc_lib  everything except main(), so the unit tests can link the real code
#   tfc      the command-line executable

# version.hpp carries the project version into the compiler at build time.
configure_file(src/version.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/generated/version.hpp @ONLY)

add_library(tfc_lib STATIC
    src/driver/command_line.cpp)

target_include_directories(tfc_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated)

tf_set_warnings(tfc_lib)

add_executable(tfc src/main.cpp)
target_link_libraries(tfc PRIVATE tfc_lib)
tf_set_warnings(tfc)
