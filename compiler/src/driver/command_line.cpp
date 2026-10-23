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

bool startsWith(const std::string &text, const std::string &prefix)
{
    return text.rfind(prefix, 0) == 0;
}

struct ValueOption
{
    const char *name; // "-o", "--cc", ...
    std::string CommandLine::*field;
};

const ValueOption kValueOptions[] = {
    {"-o", &CommandLine::outputPath},
    {"--cc", &CommandLine::cCompiler},
    {"--runtime", &CommandLine::runtimeDir},
};

} // namespace

CommandLine parseCommandLine(const std::vector<std::string> &args)
{
    CommandLine result;
    bool optionsEnded = false;
    std::vector<std::string> seenValueOptions;

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
            continue;
        }
        if (arg == "-h" || arg == "--help")
        {
            result.showHelp = true;
            continue;
        }
        if (arg == "--version")
        {
            result.showVersion = true;
            continue;
        }
        if (arg == "--emit-c")
        {
            result.emitC = true;
            continue;
        }
        if (arg == "--keep-c")
        {
            result.keepC = true;
            continue;
        }
        if (arg == "--release")
        {
            result.release = true;
            continue;
        }
        if (arg == "-v" || arg == "--verbose")
        {
            result.verbose = true;
            continue;
        }

        bool matched = false;
        for (const ValueOption &option : kValueOptions)
        {
            const std::string name = option.name;
            if (arg != name && !startsWith(arg, name + "="))
            {
                continue;
            }
            matched = true;

            for (const std::string &seen : seenValueOptions)
            {
                if (seen == name)
                {
                    fail(result, name + " given more than once");
                }
            }
            seenValueOptions.push_back(name);

            std::string value;
            if (arg == name)
            {
                // The value is the next argument; an option there is a mistake,
                // not a value. A lone "-" is a value (standard output).
                if (i + 1 >= args.size() || (startsWith(args[i + 1], "-") && args[i + 1] != "-"))
                {
                    fail(result, name + " needs a value");
                    break;
                }
                value = args[++i];
            }
            else
            {
                value = arg.substr(name.size() + 1);
                if (value.empty())
                {
                    fail(result, name + " needs a value");
                    break;
                }
            }
            result.*option.field = value;
            break;
        }
        if (!matched)
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
    return "Usage: tfc [options] <file.bas>\n"
           "\n"
           "Compiles a TermForms BASIC program to a native executable, via C.\n"
           "\n"
           "Options:\n"
           "  -o <file>         Name of the executable (or, with --emit-c, the C file;\n"
           "                    '-' writes the C to standard output)\n"
           "  --emit-c          Write the generated C and stop\n"
           "  --keep-c          Keep the generated C next to the executable\n"
           "  --release         Optimise and leave out debug information\n"
           "  --cc <command>    C compiler command, e.g. \"zig cc -target x86_64-linux-musl\"\n"
           "                    (default: $TFC_CC, else cc; on Windows zig cc or cl)\n"
           "  --runtime <dir>   TermForms runtime directory (default: $TERMFORMS_RUNTIME,\n"
           "                    else the runtime this tfc was built with)\n"
           "  -v, --verbose     Show the C compiler command\n"
           "  -h, --help        Show this help and exit\n"
           "  --version         Show the compiler version and exit\n";
}

} // namespace tf::driver
