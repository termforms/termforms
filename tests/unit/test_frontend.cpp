// SPDX-License-Identifier: BSD-2-Clause
//
// Unit tests for the lexer, parser and semantic checks, driven by small
// BASIC sources. Whole-program behaviour is covered by the golden and run
// tests; these pin down individual rules and their error messages.

#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "sema/sema.hpp"

#include <doctest.h>

#include <string>

namespace
{

// Runs the front end and returns every diagnostic as "line:col: message",
// one per line, warnings marked "warning: ".
std::string check(const std::string &source)
{
    tf::Diagnostics diagnostics("test.bas");
    const auto tokens = tf::tokenize(source, diagnostics);
    tf::ast::Module module = tf::parseModule(tokens, diagnostics);
    if (!diagnostics.hasErrors())
    {
        tf::checkModule(module, diagnostics);
    }
    std::string out;
    for (const tf::Diagnostic &d : diagnostics.all())
    {
        out += std::to_string(d.location.line) + ":" + std::to_string(d.location.column) + ": " +
               (d.severity == tf::Severity::Warning ? "warning: " : "") + d.message + "\n";
    }
    return out;
}

// A minimal form so programs have something to run.
const std::string kForm = "Begin Form f\n"
                          "    Begin TextBox t\n"
                          "    End\n"
                          "    Begin CommandButton b\n"
                          "    End\n"
                          "End\n";

} // namespace

TEST_CASE("lexer: keywords are case-insensitive, identifiers keep their case")
{
    tf::Diagnostics d("x.bas");
    const auto tokens = tf::tokenize("dim Name$ AS string", d);
    REQUIRE(tokens.size() >= 4);
    CHECK(tokens[0].isKeyword("DIM"));
    CHECK(tokens[1].kind == tf::TokenKind::Identifier);
    CHECK(tokens[1].text == "Name");
    CHECK(tokens[1].suffix == '$');
    CHECK(tokens[2].isKeyword("AS"));
    CHECK(tokens[3].isKeyword("STRING"));
    CHECK_FALSE(d.hasErrors());
}

TEST_CASE("lexer: strings, comments and continuation")
{
    tf::Diagnostics d("x.bas");
    const auto tokens = tf::tokenize("x = \"say \"\"hi\"\"\" ' comment\nREM also a comment\n"
                                     "y = 1 + _\n  2",
                                     d);
    CHECK_FALSE(d.hasErrors());
    REQUIRE(tokens.size() > 3);
    CHECK(tokens[2].kind == tf::TokenKind::StringLiteral);
    CHECK(tokens[2].text == "say \"hi\"");
    // "y = 1 + 2" is one statement despite the line break.
    int statements = 0;
    for (const auto &t : tokens)
    {
        statements += t.kind == tf::TokenKind::EndOfStatement ? 1 : 0;
    }
    CHECK(statements == 2);
}

TEST_CASE("lexer: unterminated string")
{
    CHECK(check("Sub Main()\n    x = \"oops\nEnd Sub\n") ==
          "2:9: string is missing its closing quote\n");
}

TEST_CASE("a well-formed program has no diagnostics")
{
    CHECK(check(kForm + "Dim n As Integer\n"
                        "Sub b_Click()\n"
                        "    Dim s As String\n"
                        "    s = t.Text & \" \" & n\n"
                        "    If s = \"\" Then\n"
                        "        n = n + 1\n"
                        "    ElseIf n > 3 And Not (n = 5) Then\n"
                        "        End\n"
                        "    Else\n"
                        "        t.Text = s\n"
                        "    End If\n"
                        "End Sub\n") == "");
}

TEST_CASE("Option Explicit: undeclared variables are errors")
{
    CHECK(check(kForm + "Sub b_Click()\n    x = 1\nEnd Sub\n") ==
          "8:5: 'x' is not declared (add a Dim)\n");
}

TEST_CASE("Dim needs a type")
{
    CHECK(check(kForm + "Dim x\n") ==
          "7:5: 'x' needs a type: add 'As Integer', 'As String' or 'As Boolean'\n");
}

TEST_CASE("Dim a, b As String gives both the type")
{
    CHECK(check(kForm + "Sub b_Click()\n    Dim p, q As String\n    p = q\nEnd Sub\n") == "");
}

TEST_CASE("identifiers are case-sensitive, with a hint")
{
    CHECK(check(kForm + "Sub b_Click()\n    Dim count As Integer\n    Count = 1\nEnd Sub\n") ==
          "9:5: 'Count' is not declared; did you mean 'count'? (names are case-sensitive)\n");
}

TEST_CASE("Option Strict: no implicit Integer to String")
{
    CHECK(check(kForm + "Sub b_Click()\n    t.Text = 5\nEnd Sub\n") ==
          "8:14: cannot assign an Integer to a String (Option Strict); use \"\" & value to "
          "convert\n");
}

TEST_CASE("& converts Integers to text")
{
    CHECK(check(kForm + "Sub b_Click()\n    t.Text = \"n=\" & 5\nEnd Sub\n") == "");
}

TEST_CASE("If needs a Boolean condition")
{
    CHECK(check(kForm + "Sub b_Click()\n    If 1 Then End\nEnd Sub\n") ==
          "8:8: an If condition must be Boolean, not Integer\n");
}

TEST_CASE("unknown property, with a suggestion")
{
    CHECK(check(kForm + "Sub b_Click()\n    t.text = \"x\"\nEnd Sub\n") ==
          "8:7: TextBox has no property 'text' (did you mean 'Text'?)\n");
}

TEST_CASE("event handler whose case differs is a warning")
{
    CHECK(check(kForm + "Sub B_Click()\nEnd Sub\n") ==
          "7:1: warning: Sub 'B_Click' looks like an event handler for 'b' but the case "
          "differs; it will not be called\n");
}

TEST_CASE("event name with the wrong case is a warning")
{
    CHECK(check(kForm + "Sub b_click()\nEnd Sub\n") ==
          "7:1: warning: Sub 'b_click' will not be called: the event is 'Click', not 'click'\n");
}

TEST_CASE("calls resolve to Subs, case-sensitively")
{
    CHECK(check(kForm + "Sub Helper()\nEnd Sub\nSub b_Click()\n    helper\nEnd Sub\n") ==
          "10:5: no Sub named 'helper' (did you mean 'Helper'? names are case-sensitive)\n");
}

TEST_CASE("a program needs a form or Sub Main")
{
    CHECK(check("Sub Other()\nEnd Sub\n") ==
          "1:1: nothing to run: add a form (Begin Form ... End) or a Sub Main\n");
    CHECK(check("Sub Main()\nEnd Sub\n") == "");
}

TEST_CASE("form property values are checked")
{
    CHECK(check("Begin Form f\n    Caption = 5\nEnd\n") ==
          "2:5: property 'Caption' needs a String, not an Integer\n");
}

TEST_CASE("Double is reported as not supported yet")
{
    CHECK(check(kForm + "Sub b_Click()\n    Dim n As Integer\n    n = 6 / 2\nEnd Sub\n") ==
          "9:11: '/' produces a Double, which is not supported yet; use '\\' for integer "
          "division\n");
}
