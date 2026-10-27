// SPDX-License-Identifier: BSD-2-Clause
//
// build.cpp -- see build.hpp.

#include "driver/build.hpp"

#include "codegen/c_emitter.hpp"
#include "driver/project.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "sema/sema.hpp"
#include "version.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
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
                (kWindowsHost && (fs::exists(fs::path(dir) / (name + ".exe"), ec) ||
                                  fs::exists(fs::path(dir) / (name + ".cmd"), ec) ||
                                  fs::exists(fs::path(dir) / (name + ".bat"), ec))))
            {
                return true;
            }
        }
        start = end + 1;
    }
    return false;
}

// Can the shell run the program a compiler command starts with ("zig" in
// "zig cc -target ...", a quoted path)? Asked before running it, because
// the shells' "no such command" statuses are not dependable (cmd.exe's
// varies with redirection).
bool canRun(const std::string &command)
{
    std::string program;
    std::size_t i = command.find_first_not_of(' ');
    if (i == std::string::npos)
    {
        return false;
    }
    if (command[i] == '"')
    {
        const std::size_t close = command.find('"', i + 1);
        program =
            command.substr(i + 1, close == std::string::npos ? std::string::npos : close - i - 1);
    }
    else
    {
        program = command.substr(i, command.find(' ', i) - i);
    }
    std::error_code ec;
    if (program.find('/') != std::string::npos || program.find('\\') != std::string::npos)
    {
        return fs::exists(program, ec) || (kWindowsHost && fs::exists(program + ".exe", ec));
    }
    return onPath(program);
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
        // /w: warnings about generated C are tfc's to fix (the tests compile
        // it with every warning an error), never the BASIC programmer's.
        command += " /nologo /w /utf-8 /D_CRT_SECURE_NO_WARNINGS";
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
        command += " -std=c99 -w"; // -w: as /w above
        command += release ? " -O2 -DNDEBUG" : " -O0 -g";
        command += " -I" + quote(includeDir);
        for (const std::string &s : sources)
        {
            command += " " + quote(s);
        }
        // The runtime's Double arithmetic needs pow and fmod, which glibc
        // keeps in libm; elsewhere (macOS, musl, MinGW via zig) -lm is
        // accepted and costs nothing. Libraries go after the sources.
        command += " -lm";
        command += " -o " + quote(output);
    }
    return command;
}

// What a .vbp says about its files must hold: Form= files have a form,
// Module= files none, and Startup= names the form or Sub Main.
static bool checkProjectKinds(const driver::Project &project, const std::string &projectPath,
                              const std::vector<ast::Module> &modules, std::ostream &err)
{
    bool ok = true;
    const ast::Module *formModule = nullptr;
    bool hasMain = false;
    for (std::size_t i = 0; i < modules.size(); ++i)
    {
        const driver::ProjectFile &f = project.files[i];
        const bool hasForm = modules[i].form != nullptr;
        if (hasForm != f.isForm)
        {
            err << projectPath << ":" << f.line << ": error: " << f.path
                << (f.isForm ? " is listed as a Form but has no Begin Form block"
                             : " has a form: list it as Form=, not Module=")
                << "\n";
            ok = false;
        }
        formModule = hasForm ? &modules[i] : formModule;
        for (const auto &proc : modules[i].procedures)
        {
            hasMain = hasMain || proc->name == "Main";
        }
    }
    if (!project.startup.empty())
    {
        const std::string &s = project.startup;
        const bool sayMain = s == "Sub Main" || s == "Main";
        const bool matches = sayMain ? (hasMain && formModule == nullptr)
                                     : formModule != nullptr && formModule->form->name == s;
        if (!matches)
        {
            err << projectPath << ":" << project.startupLine << ": error: Startup=" << s
                << " is not where the program starts: "
                << (formModule != nullptr ? "it starts at its form, " + formModule->form->name
                                          : std::string("it starts at Sub Main"))
                << "\n";
            ok = false;
        }
    }
    return ok;
}

// A module's name is its file's name without .bas; with several modules
// it qualifies their names (Utils.Clean), so it must be a BASIC name.
static bool isBasicName(const std::string &name)
{
    if (name.empty() || std::isalpha(static_cast<unsigned char>(name[0])) == 0)
    {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
    });
}

int runBuild(const CommandLine &cmd, std::ostream &out, std::ostream &err)
{
    if (cmd.inputs.empty())
    {
        err << "tfc: error: no input files\n";
        return kExitUsage;
    }
    // A .vbp names the files (relative to its folder) and the program.
    std::vector<std::string> inputs = cmd.inputs;
    driver::Project project;
    std::string projectPath;
    const auto isProject = [](const std::string &f) {
        return fs::path(f).extension().string() == ".vbp";
    };
    if (std::any_of(inputs.begin(), inputs.end(), isProject))
    {
        if (inputs.size() != 1)
        {
            err << "tfc: error: give a project file (.vbp) on its own, not with other files\n";
            return kExitUsage;
        }
        projectPath = inputs.front();
        std::string text;
        if (!readFile(projectPath, text))
        {
            err << "tfc: error: cannot read '" << projectPath << "'\n";
            return kExitUsage;
        }
        project = driver::parseProject(text);
        for (const std::string &e : project.errors)
        {
            // "line N: x" -> "file:N: error: x", as every diagnostic reads
            const std::size_t colon = e.find(':');
            err << projectPath << ":" << e.substr(5, colon - 5) << ": error:" << e.substr(colon + 1)
                << "\n";
        }
        if (!project.ok())
        {
            return kExitCompile;
        }
        inputs.clear();
        const fs::path folder = fs::path(projectPath).parent_path();
        for (const driver::ProjectFile &f : project.files)
        {
            const fs::path p(f.path);
            // With / as written (Windows takes it too), so messages read alike.
            inputs.push_back(p.is_absolute() ? f.path : (folder / p).generic_string());
        }
    }
    const std::string &input = inputs.front();

    // -- Front end: every file, then the program checked as a whole ----------
    // The files are the program's modules, in order: module variables are
    // set up in this order too.
    Diagnostics diagnostics(input);
    std::vector<ast::Module> modules;
    std::map<std::string, std::string> names; // a module's name, any case -> its file
    for (std::size_t i = 0; i < inputs.size(); ++i)
    {
        const std::string &file = inputs[i];
        std::string source;
        if (!readFile(file, source))
        {
            err << "tfc: error: cannot read '" << file << "'\n";
            return kExitUsage;
        }
        const std::string name = fs::path(file).stem().string();
        if (inputs.size() > 1 && !isBasicName(name))
        {
            err << "tfc: error: '" << file << "': a module's file name must be a BASIC name "
                << "(letters, digits and _, starting with a letter), as it names the module\n";
            return kExitUsage;
        }
        // Module names ignore case, as the file names they come from do on
        // Windows and macOS: Stock.bas and stock.bas would be one module.
        std::string key = name;
        for (char &c : key)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (auto [it, added] = names.emplace(key, file); !added)
        {
            err << "tfc: error: '" << it->second << "' and '" << file
                << "' would both be the module '" << name
                << "' (module names ignore case, as file names do on Windows and macOS)\n";
            return kExitCompile;
        }
        const int index = i == 0 ? 0 : diagnostics.addFile(file);
        const std::vector<Token> tokens = tokenize(source, diagnostics, index);
        modules.push_back(parseModule(tokens, diagnostics));
        modules.back().name = name;
    }
    std::vector<ast::Module *> program;
    for (ast::Module &m : modules)
    {
        program.push_back(&m);
    }
    if (!diagnostics.hasErrors())
    {
        checkProgram(program, diagnostics);
    }
    diagnostics.print(err);
    if (diagnostics.hasErrors())
    {
        return kExitCompile;
    }
    if (!projectPath.empty() && !checkProjectKinds(project, projectPath, modules, err))
    {
        return kExitCompile;
    }

    EmitOptions options;
    options.sourcePath = input;
    options.sourcePaths = inputs;
    options.version = kVersion;
    const std::string cSource =
        emitC(std::vector<const ast::Module *>(program.begin(), program.end()), options);

    // -- --emit-c: write the C and stop --------------------------------------
    // Outputs are named after the first file, or for a project after its
    // Name= (or the .vbp), in the project's folder.
    const fs::path inputPath =
        projectPath.empty()
            ? fs::path(input)
            : fs::path(projectPath).parent_path() /
                  (project.name.empty() ? fs::path(projectPath).stem().string() + ".bas"
                                        : project.name + ".bas");
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
    if (!canRun(compiler))
    {
        err << "tfc: error: cannot run the C compiler '" << compiler
            << "'; is it installed? (choose one with --cc or TFC_CC)\n";
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
    // The C compiler's messages go to a log, not the terminal: they are about
    // C the programmer did not write. Shown with -v, or kept on failure.
    const std::string logFile = output + ".tf-log";
    if (!cmd.verbose)
    {
        command += " > " + quote(logFile) + " 2>&1";
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
    if (status == 0)
    {
        if (!cmd.keepC)
        {
            fs::remove(cFile, ec);
        }
        fs::remove(logFile, ec);
        return kExitOk;
    }

    // The program passed every check, so a C compiler that runs but fails
    // means tfc wrote bad C (or the compiler is broken): either way not the
    // programmer's mistake, so say so in their terms, and keep what a bug
    // report needs.
    err << "tfc: internal error: the C compiler '" << compiler
        << "' could not build the C that tfc made from '" << inputPath.string() << "'.\n"
        << "This is a problem in tfc or in the C compiler, not in your program. Please\n"
        << "report it with your program and these files: " << cFile
        << (cmd.verbose ? "" : " and " + logFile + " (the C compiler's messages)") << "\n";
    return kExitCompile;
}

} // namespace tf::driver
