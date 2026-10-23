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
    CHECK(parseCommandLine({"hello.bas", "-o"}).error == "-o needs a file name");
    CHECK(parseCommandLine({"-o=", "hello.bas"}).error == "-o needs a file name");
    CHECK(parseCommandLine({"-o", "--version"}).error == "-o needs a file name");
    CHECK(parseCommandLine({"-o", "a", "-o", "b", "x.bas"}).error == "-o given more than once");
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
