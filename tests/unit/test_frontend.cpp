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
          "10:5: no Sub or Function named 'helper' (did you mean 'Helper'? names are "
          "case-sensitive)\n");
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

// -- Procedures, calls and loops (Phase 3 step 1) --------------------------

namespace
{

// A Sub b_Click with the given body, after kForm and any declarations.
std::string inClick(const std::string &declarations, const std::string &body)
{
    return kForm + declarations + "Sub b_Click()\n" + body + "End Sub\n";
}

const std::string kShow = "Sub Show(n As Integer, s As String)\nEnd Sub\n";

} // namespace

TEST_CASE("the three call syntaxes mean the same; parentheses are only grouping")
{
    const std::string body = "    Dim s As String\n"
                             "    Show 1, s\n"
                             "    Show(1, s)\n"
                             "    Call Show(1, s)\n"
                             "    Show (1 + 2) * 3, s\n";
    CHECK(check(inClick(kShow, body)) == "");
}

TEST_CASE("ByVal is the default and ByRef needs a variable of the same type")
{
    const std::string decl = "Sub Inc(ByRef n As Integer)\n    n = n + 1\nEnd Sub\n"
                             "Sub Change(n As Integer)\n    n = 5\nEnd Sub\n";
    CHECK(check(inClick(decl, "    Dim k As Integer\n    Inc k\n    Change k + 1\n")) == "");
    CHECK(check(inClick(decl, "    Inc 3\n")) ==
          "14:9: argument 1 of 'Inc' is ByRef, so it must be a variable\n");
    CHECK(check(inClick(decl, "    Inc t.Left\n")) ==
          "14:11: argument 1 of 'Inc' is ByRef, so it must be a variable\n");
}

TEST_CASE("a Function returns by assigning its name or with Return")
{
    const std::string decl = "Function A() As Integer\n    A = 1\nEnd Function\n"
                             "Function B%()\n    Return A + A()\nEnd Function\n";
    CHECK(check(inClick(decl, "    Dim k As Integer\n    k = A + B\n")) == "");
    // Inside its body the bare name is the result; a recursive call needs ().
    CHECK(check(kForm + "Function F(n As Integer) As Integer\n    F = F + F(n - 1)\n"
                        "End Function\n") == "");
}

TEST_CASE("a Function needs a type, a Sub cannot have one")
{
    CHECK(check(kForm + "Function F()\nEnd Function\n") ==
          "7:10: Function 'F' needs a type: add 'As Integer', 'As String' or 'As Boolean'\n");
    CHECK(check(kForm + "Sub S() As Integer\nEnd Sub\n") ==
          "7:9: a Sub returns no value; use a Function to return one\n");
}

TEST_CASE("block scope: reuse in sibling blocks, no hiding, no use outside")
{
    const std::string siblings = "    If True Then\n        Dim x As Integer\n    End If\n"
                                 "    If True Then\n        Dim x As String\n    End If\n";
    CHECK(check(inClick("", siblings)) == "");
    CHECK(check(inClick("", "    Dim x As Integer\n    If True Then\n"
                            "        Dim x As Integer\n    End If\n")) ==
          "10:13: 'x' is already declared in this Sub\n");
    CHECK(check(inClick("", "    If True Then\n        Dim x As Integer\n    End If\n"
                            "    x = 1\n")) == "11:5: 'x' is not declared (add a Dim)\n");
}

TEST_CASE("While loops end with Wend or End While")
{
    CHECK(check(inClick("", "    While False\n    Wend\n    While False\n    End While\n")) == "");
}

TEST_CASE("Exit finds the nearest loop of its kind, through other loops")
{
    const std::string body = "    Dim i As Integer\n"
                             "    For i = 1 To 2\n        Do\n            Exit For\n        Loop\n"
                             "    Next\n";
    CHECK(check(inClick("", body)) == "");
    CHECK(check(inClick("", "    Do\n        Exit While\n    Loop\n")) ==
          "9:9: 'Exit While' is not inside a While loop\n");
}

TEST_CASE("event handlers and Main take no parameters")
{
    CHECK(check(kForm + "Sub b_Click(n As Integer)\nEnd Sub\n") ==
          "7:1: event handler 'b_Click' takes no parameters\n");
    CHECK(check("Sub Main(n As Integer)\nEnd Sub\n") ==
          "1:1: Main must be a Sub with no parameters\n");
}

TEST_CASE("lexer: ByRef, Step, Until and Wend are reserved")
{
    for (const char *word : {"byref", "STEP", "Until", "wend"})
    {
        CHECK(tf::isReservedKeyword(word));
    }
}
