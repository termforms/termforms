// SPDX-License-Identifier: BSD-2-Clause
//
// command_line.hpp -- parsing of the tfc command line.
//
// Parsing is kept separate from acting on the result, so the rules can be
// unit-tested without running the compiler. The parser never prints and never
// exits; it reports problems through CommandLine::error.

#pragma once

#include <string>
#include <vector>

namespace tf::driver
{

// The parsed command line.
struct CommandLine
{
    bool showHelp = false;           // -h, --help
    bool showVersion = false;        // --version
    bool emitC = false;              // --emit-c: write the generated C and stop
    bool keepC = false;              // --keep-c: keep the generated C beside the program
    bool release = false;            // --release: optimise, no debug information
    bool verbose = false;            // -v, --verbose: show the C compiler command
    std::string outputPath;          // -o <file>; empty means "derive from the input"
    std::string cCompiler;           // --cc <command>; empty means "choose a default"
    std::string runtimeDir;          // --runtime <dir>; empty means "the default"
    std::vector<std::string> inputs; // source (.bas) files, in order
    std::string error;               // first problem found; empty when parsing succeeded

    bool ok() const { return error.empty(); }
};

// Parses the arguments that follow the program name (argv[1] onwards).
//
// Rules:
//   * Options and inputs may be mixed in any order.
//   * "--" ends option processing; everything after it is an input.
//   * -h/--help and --version need no inputs; otherwise at least one is required.
//   * Options with a value take it as the next argument or after '=':
//     "-o file", "-o=file", "--cc 'zig cc'", "--cc=zig cc". Each may appear once.
CommandLine parseCommandLine(const std::vector<std::string> &args);

// Text printed for --help.
std::string usageText();

} // namespace tf::driver
