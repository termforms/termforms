// SPDX-License-Identifier: BSD-2-Clause
//
// build.cpp -- see build.hpp.

#include "driver/build.hpp"

#include "codegen/c_emitter.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "sema/sema.hpp"
#include "version.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace tf::driver
{

namespace
{

#ifdef _WIN32
constexpr bool kWindowsHost = true;
#else
constexpr bool kWindowsHost = false;
#endif

bool readFile(const std::string &path, std::string &contents)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    contents = buffer.str();
    return true;
}

bool writeFile(const std::string &path, const std::string &contents)
{
    std::ofstream out(path, std::ios::binary);
    out << contents;
    return static_cast<bool>(out);
}

std::string getEnv(const char *name)
{
    const char *value = std::getenv(name);
    return value != nullptr ? value : "";
}

// Finds an executable on PATH; returns true if present.
bool onPath(const std::string &name)
{
    const std::string path = getEnv("PATH");
    const char separator = kWindowsHost ? ';' : ':';
    std::size_t start = 0;
    while (start <= path.size())
    {
        const std::size_t end = std::min(path.find(separator, start), path.size());
        const std::string dir = path.substr(start, end - start);
        if (!dir.empty())
        {
            std::error_code ec;
            if (fs::exists(fs::path(dir) / name, ec) ||
                (kWindowsHost && fs::exists(fs::path(dir) / (name + ".exe"), ec)))
            {
                return true;
            }
        }
        start = end + 1;
    }
    return false;
}

// --cc, then $TFC_CC, then the platform default.
std::string chooseCompiler(const CommandLine &cmd, std::string &problem)
{
    if (!cmd.cCompiler.empty())
    {
        return cmd.cCompiler;
    }
    const std::string fromEnv = getEnv("TFC_CC");
    if (!fromEnv.empty())
    {
        return fromEnv;
    }
    if constexpr (!kWindowsHost)
    {
        return "cc";
    }
    else
    {
        // Windows: zig cc needs no developer prompt; cl only works inside one.
        if (onPath("zig"))
        {
            return "zig cc";
        }
        if (onPath("cl"))
        {
            return "cl";
        }
        problem = "no C compiler found: install zig (and put it on PATH), run tfc from a "
                  "Visual Studio developer prompt, or name a compiler with --cc";
        return {};
    }
}

// --runtime, then $TERMFORMS_RUNTIME, then the runtime this tfc was built with.
std::string chooseRuntime(const CommandLine &cmd)
{
    if (!cmd.runtimeDir.empty())
    {
        return cmd.runtimeDir;
    }
    const std::string fromEnv = getEnv("TERMFORMS_RUNTIME");
    if (!fromEnv.empty())
    {
        return fromEnv;
    }
    return TF_DEFAULT_RUNTIME_DIR;
}

// Every .c file under <runtime>/src, sorted so builds are reproducible.
std::vector<std::string> runtimeSources(const std::string &runtimeDir)
{
    std::vector<std::string> sources;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(fs::path(runtimeDir) / "src", ec), end;
         !ec && it != end; it.increment(ec))
    {
        if (it->is_regular_file(ec) && it->path().extension() == ".c")
        {
            sources.push_back(it->path().string());
        }
    }
    std::sort(sources.begin(), sources.end());
    return sources;
}

// Quotes one argument for the shell that std::system uses.
std::string quote(const std::string &arg)
{
    if constexpr (kWindowsHost)
    {
        return "\"" + arg + "\""; // Windows file names cannot contain '"'
    }
    else
    {
        std::string out = "'";
        for (char c : arg)
        {
            out += c == '\'' ? std::string("'\\''") : std::string(1, c);
        }
        return out + "'";
    }
}

std::string lowerFileStem(const std::string &word)
{
    std::string stem = fs::path(word).stem().string();
    std::transform(stem.begin(), stem.end(), stem.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return stem;
}

} // namespace

CompilerStyle compilerStyle(const std::string &command)
{
    const std::string first = command.substr(0, command.find(' '));
    const std::string stem = lowerFileStem(first);
    return stem == "cl" || stem == "clang-cl" ? CompilerStyle::Msvc : CompilerStyle::Gcc;
}

std::string compileCommand(const std::string &compiler, CompilerStyle style,
                           const std::vector<std::string> &sources, const std::string &includeDir,
                           const std::string &output, const std::string &objectDir, bool release)
{
    // The compiler command is used as written: it may carry its own
    // arguments, e.g. "zig cc -target x86_64-linux-musl".
    std::string command = compiler;
    if (style == CompilerStyle::Msvc)
    {
        command += " /nologo /utf-8 /D_CRT_SECURE_NO_WARNINGS";
        command += release ? " /O2 /DNDEBUG" : " /Od /Z7";
        command += " /I" + quote(includeDir);
        for (const std::string &s : sources)
        {
            command += " " + quote(s);
        }
        // Doubled: a single backslash before the closing quote would escape it.
        command += " /Fo" + quote(objectDir + "\\\\");
        command += " /Fe" + quote(output);
        if (!release)
        {
            command += " /link /DEBUG";
        }
    }
    else
    {
        command += " -std=c99";
        command += release ? " -O2 -DNDEBUG" : " -O0 -g";
        command += " -I" + quote(includeDir);
        for (const std::string &s : sources)
        {
            command += " " + quote(s);
        }
        command += " -o " + quote(output);
    }
    return command;
}

int runBuild(const CommandLine &cmd, std::ostream &out, std::ostream &err)
{
    if (cmd.inputs.size() != 1)
    {
        err << "tfc: error: one source file per build in this version\n";
        return kExitUsage;
    }
    const std::string &input = cmd.inputs.front();

    std::string source;
    if (!readFile(input, source))
    {
        err << "tfc: error: cannot read '" << input << "'\n";
        return kExitUsage;
    }

    // -- Front end -----------------------------------------------------------
    Diagnostics diagnostics(input);
    const std::vector<Token> tokens = tokenize(source, diagnostics);
    ast::Module module = parseModule(tokens, diagnostics);
    if (!diagnostics.hasErrors())
    {
        checkModule(module, diagnostics);
    }
    diagnostics.print(err);
    if (diagnostics.hasErrors())
    {
        return kExitCompile;
    }

    EmitOptions options;
    options.sourcePath = input;
    options.version = kVersion;
    const std::string cSource = emitC(module, options);

    // -- --emit-c: write the C and stop --------------------------------------
    const fs::path inputPath(input);
    if (cmd.emitC)
    {
        const std::string target = cmd.outputPath.empty()
                                       ? fs::path(inputPath).replace_extension(".c").string()
                                       : cmd.outputPath;
        if (target == "-")
        {
            out << cSource;
            return kExitOk;
        }
        if (!writeFile(target, cSource))
        {
            err << "tfc: error: cannot write '" << target << "'\n";
            return kExitCompile;
        }
        return kExitOk;
    }

    // -- Back end: C compiler ----------------------------------------------
    std::string problem;
    const std::string compiler = chooseCompiler(cmd, problem);
    if (compiler.empty())
    {
        err << "tfc: error: " << problem << '\n';
        return kExitCompile;
    }

    const std::string runtimeDir = chooseRuntime(cmd);
    const std::vector<std::string> runtime = runtimeSources(runtimeDir);
    if (runtime.empty())
    {
        err << "tfc: error: no TermForms runtime found in '" << runtimeDir
            << "' (use --runtime or set TERMFORMS_RUNTIME)\n";
        return kExitCompile;
    }

    std::string output = cmd.outputPath;
    if (output.empty())
    {
        output = fs::path(inputPath).replace_extension(kWindowsHost ? ".exe" : "").string();
    }

    // The generated C sits next to the program while it is compiled.
    const std::string cFile = output + ".tf.c";
    if (!writeFile(cFile, cSource))
    {
        err << "tfc: error: cannot write '" << cFile << "'\n";
        return kExitCompile;
    }

    std::vector<std::string> sources{cFile};
    sources.insert(sources.end(), runtime.begin(), runtime.end());

    const CompilerStyle style = compilerStyle(compiler);
    const std::string objectDir = output + ".tf-obj";
    std::error_code ec;
    if (style == CompilerStyle::Msvc)
    {
        fs::create_directories(objectDir, ec);
    }

    const std::string includeDir = (fs::path(runtimeDir) / "include").string();
    std::string command =
        compileCommand(compiler, style, sources, includeDir, output, objectDir, cmd.release);
    if (cmd.verbose)
    {
        err << command << '\n';
    }
    // cmd.exe strips one pair of outer quotes when the line starts with one.
    if constexpr (kWindowsHost)
    {
        command = "\"" + command + "\"";
    }

    out.flush();
    err.flush();
    const int status = std::system(command.c_str());

    if (style == CompilerStyle::Msvc)
    {
        fs::remove_all(objectDir, ec);
    }
    if (!cmd.keepC)
    {
        fs::remove(cFile, ec);
    }
    if (status != 0)
    {
        err << "tfc: error: the C compiler failed (run with -v to see the command)\n";
        return kExitCompile;
    }
    return kExitOk;
}

} // namespace tf::driver
