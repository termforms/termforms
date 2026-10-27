# SPDX-License-Identifier: BSD-2-Clause
#
# The TermForms compiler and build driver.
#
#   tfc_lib  everything except main(), so the unit tests can link the real code
#   tfc      the command-line executable

# version.hpp carries the project version into the compiler at build time.
configure_file(src/version.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/generated/version.hpp @ONLY)

add_library(tfc_lib STATIC
    src/diag/diagnostics.cpp
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/sema/catalog.cpp
    src/sema/sema.cpp
    src/codegen/c_emitter.cpp
    src/driver/command_line.cpp
    src/driver/project.cpp
    src/driver/build.cpp)

target_include_directories(tfc_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated)

# Where tfc finds the runtime sources when neither --runtime nor
# TERMFORMS_RUNTIME says otherwise: the runtime in this source tree.
target_compile_definitions(tfc_lib PRIVATE
    TF_DEFAULT_RUNTIME_DIR="${PROJECT_SOURCE_DIR}/runtime")

tf_set_warnings(tfc_lib)

add_executable(tfc src/main.cpp)
target_link_libraries(tfc PRIVATE tfc_lib)
tf_set_warnings(tfc)
