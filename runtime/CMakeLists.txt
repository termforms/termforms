# SPDX-License-Identifier: 0BSD
#
# tfrt: the TermForms runtime library, linked into every compiled program.
#
# Kept strictly C99 with no third-party dependencies: only the C standard
# library plus POSIX or Win32. That keeps the 0BSD licence clean and lets a
# compiled program be a single static binary.

add_library(tfrt STATIC
    src/core/tf_version.c)

target_include_directories(tfrt PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# The runtime reports the version of the build it came from, so a compiled
# program and the compiler that produced it can be checked against each other.
target_compile_definitions(tfrt PRIVATE TF_RUNTIME_VERSION="${PROJECT_VERSION}")

tf_set_warnings(tfrt)
