// SPDX-License-Identifier: BSD-2-Clause
//
// build.hpp -- the build driver: BASIC source to executable.
//
//   1. lex, parse and check the source (errors stop the build here)
//   2. generate C
//   3. run a C compiler on the generated C plus the runtime sources
//
// The runtime is compiled with every program rather than linked as a
// prebuilt library. It is small, and it means a cross build (e.g. zig cc to
// x86_64-linux-musl on a Mac) always gets a runtime for the right target.

#pragma once

#include "driver/command_line.hpp"

#include <ostream>
#include <string>
#include <vector>

namespace tf::driver
{

// Exit codes (stable; scripts and the test suite rely on them).
constexpr int kExitOk = 0;
constexpr int kExitUsage = 1;
constexpr int kExitCompile = 2;

// Runs a build as described by the command line. Messages go to err (and
// the generated C to out, for "--emit-c -o -"). Returns an exit code.
int runBuild(const CommandLine &cmd, std::ostream &out, std::ostream &err);

// How the C compiler expects its arguments.
enum class CompilerStyle
{
    Gcc, // gcc, clang, zig cc, cc
    Msvc // cl, clang-cl
};

// Chooses the style from the command's first word.
CompilerStyle compilerStyle(const std::string &command);

// Builds the C compiler command line. Exposed for unit tests.
std::string compileCommand(const std::string &compiler, CompilerStyle style,
                           const std::vector<std::string> &sources, const std::string &includeDir,
                           const std::string &output, const std::string &objectDir, bool release);

} // namespace tf::driver
