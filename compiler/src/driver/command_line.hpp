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
    std::string outputPath;          // -o <file>; empty means "derive from the input"
    std::vector<std::string> inputs; // source (.bas) or project (.vbp) files, in order
    std::string error;               // first problem found; empty when parsing succeeded

    bool ok() const { return error.empty(); }
};

// Parses the arguments that follow the program name (argv[1] onwards).
//
// Rules:
//   * Options and inputs may be mixed in any order.
//   * "--" ends option processing; everything after it is an input.
//   * -h/--help and --version need no inputs; otherwise at least one is required.
//   * -o may appear once and needs a value: "-o file" or "-o=file".
CommandLine parseCommandLine(const std::vector<std::string> &args);

// Text printed for --help.
std::string usageText();

} // namespace tf::driver
