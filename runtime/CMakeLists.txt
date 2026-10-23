# SPDX-License-Identifier: 0BSD
#
# tfrt: the TermForms runtime library, linked into every compiled program.
#
# Kept strictly C99 with no third-party dependencies: only the C standard
# library plus POSIX or Win32. That keeps the 0BSD licence clean and lets a
# compiled program be a single static binary.
#
# tfc does not link this library: it compiles these same sources together with
# each program, so the runtime is always built for the program's target (which
# matters when cross-compiling with zig cc). This CMake target exists so the
# runtime is compiled with full warnings on every platform and can be tested.
# Platform files guard themselves with #if, so every file is listed for every
# platform; tfc relies on that too.

set(TF_RUNTIME_SOURCES
    src/core/tf_version.c
    src/core/tf_error.c
    src/core/tf_string.c
    src/core/tf_integer.c
    src/tui/tf_keys.c
    src/tui/tf_screen.c
    src/tui/tf_input.c
    src/tui/tf_forms.c
    src/platform/posix/tf_term_posix.c
    src/platform/win32/tf_term_win32.c)

add_library(tfrt STATIC ${TF_RUNTIME_SOURCES})

target_include_directories(tfrt PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

tf_set_warnings(tfrt)
