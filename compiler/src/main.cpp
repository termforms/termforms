// SPDX-License-Identifier: BSD-2-Clause
//
// main.cpp -- entry point of tfc, the TermForms compiler and build driver.
//
// Exit codes (stable; scripts and the test suite rely on them):
//   0  success
//   1  command-line error
//   2  compilation failed

#include "driver/build.hpp"
#include "driver/command_line.hpp"
#include "version.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char *argv[])
{
    using namespace tf::driver;

    const std::vector<std::string> args(argv + 1, argv + argc);
    const CommandLine cmd = parseCommandLine(args);

    if (!cmd.ok())
    {
        std::cerr << "tfc: error: " << cmd.error << '\n'
                  << "Try 'tfc --help' for more information.\n";
        return kExitUsage;
    }

    // Help takes precedence over version, as in most command-line tools.
    if (cmd.showHelp)
    {
        std::cout << usageText();
        return kExitOk;
    }
    if (cmd.showVersion)
    {
        std::cout << "tfc (TermForms) " << tf::kVersion << '\n';
        return kExitOk;
    }

    return runBuild(cmd, std::cout, std::cerr);
}
