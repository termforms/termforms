// SPDX-License-Identifier: BSD-2-Clause
//
// main.cpp -- entry point of tfc, the TermForms compiler and build driver.
//
// Exit codes (stable; scripts and the test suite rely on them):
//   0  success
//   1  command-line error
//   2  compilation failed (or, for now, not yet implemented)

#include "driver/command_line.hpp"
#include "version.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace
{

constexpr int kExitOk = 0;
constexpr int kExitUsage = 1;
constexpr int kExitCompile = 2;

} // namespace

int main(int argc, char *argv[])
{
    const std::vector<std::string> args(argv + 1, argv + argc);
    const tf::driver::CommandLine cmd = tf::driver::parseCommandLine(args);

    if (!cmd.ok())
    {
        std::cerr << "tfc: error: " << cmd.error << '\n'
                  << "Try 'tfc --help' for more information.\n";
        return kExitUsage;
    }

    // Help takes precedence over version, as in most command-line tools.
    if (cmd.showHelp)
    {
        std::cout << tf::driver::usageText();
        return kExitOk;
    }

    if (cmd.showVersion)
    {
        std::cout << "tfc (TermForms) " << tf::kVersion << '\n';
        return kExitOk;
    }

    // The compiler pipeline arrives with the Phase 2 vertical slice.
    std::cerr << "tfc: error: compilation is not implemented yet\n";
    return kExitCompile;
}
