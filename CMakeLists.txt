# SPDX-License-Identifier: BSD-2-Clause
#
# TermForms top-level build.
#
# Layout:
#   compiler/   C++17 compiler and build driver (tfc)
#   runtime/    C99 runtime library linked into every compiled program (0BSD)
#   tests/      unit tests (doctest), runtime tests and command-line tests
#
# Configure with one of the presets in CMakePresets.json, e.g.
#   cmake --preset dev && cmake --build --preset dev && ctest --preset dev

cmake_minimum_required(VERSION 3.21)

project(TermForms
    VERSION 0.9.2
    DESCRIPTION "VB-DOS-style forms applications for the terminal, compiled to C"
    HOMEPAGE_URL "https://github.com/termforms/termforms"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Language standards. The runtime and all generated code are plain C99; the
# compiler is C++17. Extensions are off so that GCC/Clang behave like MSVC.
# ---------------------------------------------------------------------------
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit compile_commands.json for clangd and clang-tidy (ignored by VS generators).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(TF_BUILD_TESTS          "Build the test suites"                         ON)
option(TF_WARNINGS_AS_ERRORS   "Treat compiler warnings as errors (CI default)" OFF)
option(TF_SANITIZE             "Build with AddressSanitizer and UBSan (GCC/Clang only)" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(TfWarnings)
include(TfSanitizers)

# ---------------------------------------------------------------------------
# Components
# ---------------------------------------------------------------------------
add_subdirectory(runtime)
add_subdirectory(compiler)

if(TF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
