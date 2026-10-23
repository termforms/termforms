// SPDX-License-Identifier: BSD-2-Clause
//
// command_line.cpp -- see command_line.hpp.

#include "driver/command_line.hpp"

namespace tf::driver
{

namespace
{

// Records the first error only; later problems are usually consequences of it.
void fail(CommandLine &result, const std::string &message)
{
    if (result.error.empty())
    {
        result.error = message;
    }
}

bool startsWith(const std::string &text, const char *prefix)
{
    return text.rfind(prefix, 0) == 0;
}

} // namespace

CommandLine parseCommandLine(const std::vector<std::string> &args)
{
    CommandLine result;
    bool outputSeen = false;
    bool optionsEnded = false;

    for (std::size_t i = 0; i < args.size(); ++i)
    {
        const std::string &arg = args[i];

        // Anything after "--", and anything not starting with '-', is an input.
        // A lone "-" is treated as an input too (reserved for stdin later).
        if (optionsEnded || arg.empty() || arg[0] != '-' || arg == "-")
        {
            result.inputs.push_back(arg);
            continue;
        }

        if (arg == "--")
        {
            optionsEnded = true;
        }
        else if (arg == "-h" || arg == "--help")
        {
            result.showHelp = true;
        }
        else if (arg == "--version")
        {
            result.showVersion = true;
        }
        else if (arg == "-o" || startsWith(arg, "-o="))
        {
            if (outputSeen)
            {
                fail(result, "-o given more than once");
                continue;
            }
            outputSeen = true;

            if (arg == "-o")
            {
                // The value is the next argument; an option there is a mistake,
                // not a file name.
                if (i + 1 >= args.size() || startsWith(args[i + 1], "-"))
                {
                    fail(result, "-o needs a file name");
                    continue;
                }
                result.outputPath = args[++i];
            }
            else
            {
                result.outputPath = arg.substr(3);
                if (result.outputPath.empty())
                {
                    fail(result, "-o needs a file name");
                }
            }
        }
        else
        {
            fail(result, "unknown option '" + arg + "'");
        }
    }

    // Help and version are complete requests in themselves.
    if (result.ok() && !result.showHelp && !result.showVersion && result.inputs.empty())
    {
        fail(result, "no input files");
    }

    return result;
}

std::string usageText()
{
    return "Usage: tfc [options] <file.bas | project.vbp>...\n"
           "\n"
           "Options:\n"
           "  -o <file>     Name of the executable to produce\n"
           "  -h, --help    Show this help and exit\n"
           "  --version     Show the compiler version and exit\n";
}

} // namespace tf::driver
