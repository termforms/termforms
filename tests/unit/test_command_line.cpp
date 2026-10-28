// SPDX-License-Identifier: BSD-2-Clause
//
// Unit tests for tf::driver::parseCommandLine.

#include "driver/command_line.hpp"

#include <doctest.h>

using tf::driver::CommandLine;
using tf::driver::parseCommandLine;

TEST_CASE("inputs are collected in order")
{
    const CommandLine cmd = parseCommandLine({"main.bas", "util.bas"});
    REQUIRE(cmd.ok());
    REQUIRE(cmd.inputs.size() == 2);
    CHECK(cmd.inputs[0] == "main.bas");
    CHECK(cmd.inputs[1] == "util.bas");
    CHECK(cmd.outputPath.empty());
}

TEST_CASE("no arguments is an error")
{
    const CommandLine cmd = parseCommandLine({});
    CHECK_FALSE(cmd.ok());
    CHECK(cmd.error == "no input files");
}

TEST_CASE("help and version need no inputs")
{
    CHECK(parseCommandLine({"--help"}).showHelp);
    CHECK(parseCommandLine({"-h"}).showHelp);
    CHECK(parseCommandLine({"--version"}).showVersion);
    CHECK(parseCommandLine({"--version"}).ok());
}

TEST_CASE("-o takes the next argument or an = value")
{
    SUBCASE("separate argument")
    {
        const CommandLine cmd = parseCommandLine({"-o", "hello", "hello.bas"});
        REQUIRE(cmd.ok());
        CHECK(cmd.outputPath == "hello");
        CHECK(cmd.inputs.size() == 1);
    }
    SUBCASE("equals form")
    {
        const CommandLine cmd = parseCommandLine({"hello.bas", "-o=hello"});
        REQUIRE(cmd.ok());
        CHECK(cmd.outputPath == "hello");
    }
}

TEST_CASE("-o errors")
{
    CHECK(parseCommandLine({"hello.bas", "-o"}).error == "-o needs a value");
    CHECK(parseCommandLine({"-o=", "hello.bas"}).error == "-o needs a value");
    CHECK(parseCommandLine({"-o", "--version"}).error == "-o needs a value");
    CHECK(parseCommandLine({"-o", "a", "-o", "b", "x.bas"}).error == "-o given more than once");
}

TEST_CASE("-o - means standard output")
{
    const CommandLine cmd = parseCommandLine({"--emit-c", "-o", "-", "x.bas"});
    REQUIRE(cmd.ok());
    CHECK(cmd.outputPath == "-");
    CHECK(cmd.emitC);
}

TEST_CASE("--cc keeps a multi-word command intact")
{
    const CommandLine cmd = parseCommandLine({"--cc", "zig cc -target x86_64-linux-musl", "x.bas"});
    REQUIRE(cmd.ok());
    CHECK(cmd.cCompiler == "zig cc -target x86_64-linux-musl");

    const CommandLine eq = parseCommandLine({"--cc=clang", "x.bas"});
    CHECK(eq.cCompiler == "clang");
}

TEST_CASE("flags")
{
    const CommandLine cmd = parseCommandLine({"--keep-c", "--release", "-v", "x.bas"});
    REQUIRE(cmd.ok());
    CHECK(cmd.keepC);
    CHECK(cmd.release);
    CHECK(cmd.verbose);
}

TEST_CASE("unknown options are reported by name")
{
    const CommandLine cmd = parseCommandLine({"--frobnicate", "x.bas"});
    CHECK(cmd.error == "unknown option '--frobnicate'");
}

TEST_CASE("-- ends option processing")
{
    const CommandLine cmd = parseCommandLine({"--", "-odd-name.bas"});
    REQUIRE(cmd.ok());
    REQUIRE(cmd.inputs.size() == 1);
    CHECK(cmd.inputs[0] == "-odd-name.bas");
}

TEST_CASE("only the first error is kept")
{
    const CommandLine cmd = parseCommandLine({"--bad1", "--bad2"});
    CHECK(cmd.error == "unknown option '--bad1'");
}

// -- The .vbp project file ---------------------------------------------------------

#include "driver/project.hpp"

using tf::driver::parseProject;

TEST_CASE("a project lists its files in order, with a name and a startup")
{
    const auto p = parseProject("' Inventory\nName = Inventory\nstartup=frmMain\n\n"
                                "Form=frmMain.bas\nModule = lib/stock.bas \n");
    REQUIRE(p.ok());
    CHECK(p.name == "Inventory");
    CHECK(p.startup == "frmMain");
    REQUIRE(p.files.size() == 2);
    CHECK(p.files[0].path == "frmMain.bas");
    CHECK(p.files[0].isForm);
    CHECK(p.files[1].path == "lib/stock.bas");
    CHECK_FALSE(p.files[1].isForm);
    CHECK(p.files[1].line == 6);
}

TEST_CASE("a project's mistakes are reported with their lines")
{
    const auto p = parseProject("Name=A\nName=B\nColour=red\njunk\nModule=\n");
    REQUIRE(p.errors.size() == 4);
    CHECK(p.errors[0] == "line 2: Name is given twice");
    CHECK(p.errors[1].rfind("line 3: unknown key 'Colour'", 0) == 0);
    CHECK(p.errors[2].rfind("line 4: expected Key=Value", 0) == 0);
    CHECK(p.errors[3] == "line 5: 'Module' needs a value");
    CHECK(parseProject("Name=A\n").errors.at(0) ==
          "line 1: the project lists no files (Form= or Module=)");
}

// -- The C compiler's command lines (driver/build.cpp) -------------------

#include "driver/build.hpp"

#include <string>

using tf::driver::CompilerStyle;

TEST_CASE("a Declare's libraries are linked, after the sources")
{
    // Quoting differs by host (' on POSIX, " on Windows): look past it.
    const std::string gcc = tf::driver::compileCommand("cc", CompilerStyle::Gcc, {"p.tf.c"}, "inc",
                                                       "p", "obj", false, {"sqlite3", "m2"});
    CHECK(gcc.find("p.tf.c") < gcc.find("-l"));
    CHECK(gcc.find("sqlite3") > gcc.find("p.tf.c"));
    CHECK(gcc.find("m2") != std::string::npos);
    const std::string msvc = tf::driver::compileCommand("cl", CompilerStyle::Msvc, {"p.tf.c"},
                                                        "inc", "p.exe", "obj", false, {"sqlite3"});
    CHECK(msvc.find("sqlite3.lib") != std::string::npos);
}

TEST_CASE("the program's own C is compiled on its own, its warnings shown")
{
    const std::string gcc =
        tf::driver::compileOwnCCommand("zig cc -target x86_64-linux-musl", CompilerStyle::Gcc,
                                       "utils.c", "inc", "c0_utils.o", true);
    CHECK(gcc.rfind("zig cc -target x86_64-linux-musl -c", 0) == 0);
    CHECK(gcc.find(" -w") == std::string::npos); // their warnings are theirs to see
    CHECK(gcc.find("-std=") == std::string::npos);
    CHECK(gcc.find("utils.c") < gcc.find(" -o "));
    CHECK(gcc.find("c0_utils.o") > gcc.find(" -o "));
    const std::string msvc = tf::driver::compileOwnCCommand("cl", CompilerStyle::Msvc, "utils.c",
                                                            "inc", "c0_utils.obj", false);
    CHECK(msvc.find(" /c") != std::string::npos);
    CHECK(msvc.find("/Fo") < msvc.find("c0_utils.obj"));
}
