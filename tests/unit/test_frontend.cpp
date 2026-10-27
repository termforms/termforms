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
          "7:5: 'x' needs a type, such as 'As Integer' or 'As String'\n");
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
          "8:14: cannot assign an Integer to a String (Option Strict); use CStr(value) to "
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

TEST_CASE("/ always gives a Double, so storing it in an Integer needs CInt")
{
    CHECK(check(kForm + "Sub b_Click()\n    Dim n As Integer\n    n = 6 / 2\nEnd Sub\n") ==
          "9:11: cannot assign a Double to an Integer (Option Strict); use CInt(value) to "
          "convert\n");
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
          "7:10: Function 'F' needs a type, such as 'As Integer' or 'As String'\n");
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

// -- Long and Double (Phase 3 step 2) ---------------------------------------

TEST_CASE("lexer: Double, hexadecimal, octal and suffixed number literals")
{
    tf::Diagnostics d("x.bas");
    const auto tokens = tf::tokenize("1.5 2E10 2.5e-3 &HFF &o17 5& 7# 3% 2E", d);
    REQUIRE(tokens.size() >= 10);
    CHECK(tokens[0].kind == tf::TokenKind::DoubleLiteral);
    CHECK(tokens[0].real == 1.5);
    CHECK(tokens[1].real == 2e10);
    CHECK(tokens[2].real == 2.5e-3);
    CHECK(tokens[3].integer == 255);
    CHECK(tokens[4].integer == 15);
    CHECK(tokens[5].kind == tf::TokenKind::IntegerLiteral);
    CHECK(tokens[5].suffix == '&');
    CHECK(tokens[6].kind == tf::TokenKind::DoubleLiteral); // 7# is a Double
    CHECK(tokens[7].suffix == '%');
    // "2E" with no exponent digits is the number 2 followed by the name E.
    CHECK(tokens[8].integer == 2);
    CHECK(tokens[9].kind == tf::TokenKind::Identifier);
    CHECK_FALSE(d.hasErrors());
}

TEST_CASE("lexer: a fraction cannot carry an Integer or Long suffix")
{
    tf::Diagnostics d("x.bas");
    tf::tokenize("1.5&", d);
    REQUIRE(d.all().size() == 1);
    CHECK(d.all()[0].message == "a number with a fraction cannot have the suffix '&'");
}

TEST_CASE("widening is implicit, narrowing needs a conversion")
{
    const std::string decl = "Sub TakesDouble(x As Double)\nEnd Sub\n";
    const std::string widen = "    Dim i As Integer, n As Long, d As Double\n"
                              "    n = i\n    d = n\n    d = i\n    TakesDouble i\n"
                              "    i = CInt(d)\n    n = CLng(d)\n";
    CHECK(check(inClick(decl, widen)) == "");
    CHECK(check(inClick("", "    Dim i As Integer\n    i = 3000000000\n")) ==
          "9:9: cannot assign a Long to an Integer (Option Strict); use CInt(value) to "
          "convert\n");
}

TEST_CASE("arithmetic types: the wider operand wins; / and ^ give a Double")
{
    const std::string body = "    Dim i As Integer, n As Long, d As Double\n"
                             "    n = i + n\n    d = i / i\n    d = i ^ 2\n    n = n Mod i\n"
                             "    i = i \\ 2\n";
    CHECK(check(inClick("", body)) == "");
    CHECK(check(inClick("", "    Dim i As Integer\n    i = i / 2\n")) ==
          "9:11: cannot assign a Double to an Integer (Option Strict); use CInt(value) to "
          "convert\n");
}

TEST_CASE("Str and Str$ are the same built-in; others take no $")
{
    CHECK(check(inClick("", "    Dim s As String\n    s = Str(1) & Str$(2)\n")) == "");
    CHECK(check(inClick("", "    Dim d As Double\n    d = Val$(\"1\")\n")) ==
          "9:9: 'Val' is written without a '$' suffix\n");
}

// -- Try/Catch/Finally (Phase 3 step 3) --------------------------------------

TEST_CASE("a Try needs a Catch or a Finally")
{
    CHECK(check(inClick("", "    Try\n    End Try\n")) ==
          "8:5: a Try needs at least one Catch or a Finally\n");
}

TEST_CASE("Catch takes no exception variable, unlike VB.NET")
{
    CHECK(check(inClick("", "    Try\n    Catch ex As Exception\n    End Try\n")) ==
          "9:11: Catch takes no variable: the error is in Err (write Catch, or Catch When "
          "Err.Number = ...)\n");
}

TEST_CASE("Try, Throw, Err.Raise and Err.Clear are accepted in their forms")
{
    const std::string body = "    Try\n        Throw 5\n        Throw 6, \"six\"\n"
                             "        Err.Raise 7\n        Err.Raise 8, \"src\", \"eight\"\n"
                             "    Catch When Err.Number = 5\n        Throw\n"
                             "    Catch\n        Err.Clear\n    Finally\n    End Try\n";
    CHECK(check(inClick("", body)) == "");
}

TEST_CASE("lexer: Try, Throw and When are reserved")
{
    for (const char *word : {"try", "THROW", "When"})
    {
        CHECK(tf::isReservedKeyword(word));
    }
}

// -- Const and Enum (Phase 3 step 4) -----------------------------------------

TEST_CASE("a Const's type comes from its value unless As or a suffix says otherwise")
{
    const std::string decl = "Const A = 1\nConst B = 3000000000\nConst C = 0.5\nConst D = \"x\"\n"
                             "Const E As Long = 1\nConst F# = 2\n";
    const std::string body = "    Dim i As Integer, n As Long, d As Double, s As String\n"
                             "    i = A\n    n = B\n    d = C\n    s = D\n    n = E\n    d = F\n";
    CHECK(check(inClick(decl, body)) == "");
    // B is a Long because its value does not fit an Integer.
    CHECK(check(inClick("Const B = 3000000000\n", "    Dim i As Integer\n    i = B\n")) ==
          "10:9: cannot assign a Long to an Integer (Option Strict); use CInt(value) to "
          "convert\n");
}

TEST_CASE("Enum members are qualified and an Enum is its own type")
{
    const std::string decl = "Enum Color\n    Red\n    Green\nEnd Enum\n";
    CHECK(check(inClick(decl, "    Dim c As Color, i As Integer\n    c = Color.Green\n"
                              "    i = c + 1\n    c = CType(i, Color)\n")) == "");
    CHECK(check(inClick(decl, "    Dim c As Color\n    c = 1\n")) ==
          "13:9: cannot assign an Integer to a Color (Option Strict); use CType(value, Color) "
          "to convert\n");
}

TEST_CASE("an Enum is based on Integer or Long, and declared at module level")
{
    CHECK(check(kForm + "Enum E As String\n    A\nEnd Enum\n") ==
          "7:11: an Enum is based on Integer or Long\n");
    CHECK(check(inClick("", "    Enum E\n        A\n    End Enum\n")) ==
          "8:5: an Enum is declared at module level, outside any Sub or Function\n");
}

TEST_CASE("lexer: Const, Enum and CType are reserved")
{
    for (const char *word : {"const", "ENUM", "CType"})
    {
        CHECK(tf::isReservedKeyword(word));
    }
}

// -- Arrays (Phase 3 step 5a) --------------------------------------------------

TEST_CASE("arrays start at 0: no lower bound with To")
{
    CHECK(check(inClick("", "    Dim a(1 To 10) As Integer\n")) ==
          "8:13: arrays start at 0: write a(n) for elements 0 to n, not a range with 'To'\n");
}

TEST_CASE("several dimensions; a fixed array's size is a constant")
{
    CHECK(check(inClick("Const ROWS = 9\n",
                        "    Dim g(ROWS, 79) As String\n    g(1, 2) = \"x\"\n")) == "");
}

TEST_CASE("only a dynamic array can be ReDim'd, and b = a copies into a dynamic one")
{
    const std::string body = "    Dim d() As Integer, e() As Integer, fx(3) As Integer\n"
                             "    ReDim d(5)\n    ReDim Preserve d(9)\n    e = d\n    e = fx\n";
    CHECK(check(inClick("", body)) == "");
    CHECK(check(inClick("", "    Dim fx(3) As Integer\n    ReDim fx(5)\n")) ==
          "9:11: 'fx' is a fixed array; only a dynamic array (Dim fx()) can be ReDim'd\n");
}

TEST_CASE("a dynamic array declares its dimensions: () or (,)")
{
    CHECK(check(inClick("", "    Dim g(,) As Integer\n    ReDim g(2, 3)\n    g(1, 1) = 4\n")) ==
          "");
}

TEST_CASE("lexer: ReDim, Preserve and Erase are reserved")
{
    for (const char *word : {"redim", "PRESERVE", "Erase"})
    {
        CHECK(tf::isReservedKeyword(word));
    }
}

// -- Types (Phase 3 step 5b) ---------------------------------------------------

TEST_CASE("a Type may hold arrays, other Types, and be returned by a Function")
{
    const std::string decl = "Type Line\n    Qty As Integer\nEnd Type\n"
                             "Type Order\n    Lines() As Line\n    Totals(3) As Double\n"
                             "    Last As Line\nEnd Type\n"
                             "Function Make() As Order\n    ReDim Make.Lines(1)\nEnd Function\n";
    const std::string body = "    Dim o As Order, copy As Order\n    o = Make()\n"
                             "    o.Lines(0).Qty = 2\n    o.Totals(3) = 1.5\n    copy = o\n"
                             "    ReDim Preserve o.Lines(4)\n";
    CHECK(check(inClick(decl, body)) == "");
}

TEST_CASE("a Type cannot contain itself by value")
{
    CHECK(check(kForm + "Type T\n    Again As T\nEnd Type\n") ==
          "7:1: Type T contains itself (T -> T); use a Ref member, or a dynamic array, to point to "
          "another T\n");
    CHECK(check(kForm + "Type T\n    More() As T\nEnd Type\n") == "");
}

TEST_CASE("lexer: Type is reserved")
{
    CHECK(tf::isReservedKeyword("type"));
}

// -- Ref (Phase 3 step 5b) -----------------------------------------------------

TEST_CASE("Refs share, values copy, and they convert into each other by copying")
{
    const std::string decl = "Type P\n    X As Integer\n    Next As Ref P\nEnd Type\n";
    const std::string body = "    Dim r As Ref P, s As Ref P, v As P, n As New P\n"
                             "    r = New P\n    s = r\n    r = v\n    v = r\n    r = Nothing\n"
                             "    If r Is Nothing Or s IsNot r Then n.Next = s\n";
    CHECK(check(inClick(decl, body)) == "");
}

TEST_CASE("a keyword may name a member, after a dot and in a Type")
{
    CHECK(check(kForm + "Type L\n    Next As Ref L\n    Step As Integer\nEnd Type\n"
                        "Sub b_Click()\n    Dim x As New L\n    x.Step = 1\n    x.Next = x.Next\n"
                        "End Sub\n") == "");
}

TEST_CASE("lexer: Ref, New, Nothing, Is and IsNot are reserved")
{
    for (const char *word : {"ref", "NEW", "Nothing", "is", "IsNot"})
    {
        CHECK(tf::isReservedKeyword(word));
    }
}

// -- With (Phase 3 step 5b) ----------------------------------------------------

TEST_CASE("With works on a Type, a Ref, an element, a Function's result and a control")
{
    const std::string decl = "Type P\n    X As Integer\n    Q As Ref P\nEnd Type\n"
                             "Function F() As P\n    F.X = 1\nEnd Function\n";
    const std::string body = "    Dim v As P, r As New P, a(2) As P, n As Integer\n"
                             "    With v\n        .X = 1\n        With .Q\n            .X = 2\n"
                             "        End With\n    End With\n"
                             "    With r\n        .X = .X + 1\n    End With\n"
                             "    With a(n)\n        .X = 3\n    End With\n"
                             "    With F()\n        n = .X\n    End With\n"
                             "    With b\n        .Caption = \"go\"\n    End With\n";
    CHECK(check(inClick(decl, body)) == "");
}

TEST_CASE("a name then a spaced dot passes a With member; unspaced, it is a member")
{
    const std::string decl = "Type P\n    X As Integer\nEnd Type\n"
                             "Sub Bump(ByRef n As Integer)\n    n = n + 1\nEnd Sub\n";
    CHECK(check(inClick(decl, "    Dim v As P\n    With v\n        Bump .X\n    End With\n")) ==
          "");
    CHECK(check(inClick(decl, "    Dim v As P\n    v.X = 1\n    v .X\n")) != "");
}

TEST_CASE("lexer: With is reserved")
{
    CHECK(tf::isReservedKeyword("with"));
}

// -- For Each (Phase 3 step 5c) ------------------------------------------------

TEST_CASE("For Each takes each element of an array into a variable that can hold it")
{
    const std::string decl = "Type P\n    X As Integer\n    L() As String\nEnd Type\n";
    const std::string body =
        "    Dim a(2) As Integer, g(1, 1) As P, n As Long, p As P, s As String\n"
        "    For Each n In a\n        Exit For\n    Next n\n"
        "    For Each p In g\n        For Each s In p.L\n        Next\n"
        "    Next\n";
    CHECK(check(inClick(decl, body)) == "");
    CHECK(check(inClick(
              "", "    Dim a(2) As Long, n As Integer\n    For Each n In a\n    Next\n")) != "");
}

TEST_CASE("lexer: Each and In are reserved")
{
    CHECK(tf::isReservedKeyword("each"));
    CHECK(tf::isReservedKeyword("In"));
}

// -- Dictionary (Phase 3 step 5c) ----------------------------------------------

TEST_CASE("a Dictionary is declared with its value type and used by String key")
{
    const std::string decl = "Type P\n    Ages As Dictionary(Of Integer)\nEnd Type\n";
    const std::string body =
        "    Dim d As Dictionary(Of Long), p As P, k As String, ks() As String\n"
        "    d(\"a\") = 1\n    d.Add \"b\", 2\n    p.Ages(\"x\") = CInt(d(\"a\"))\n"
        "    If d.Exists(\"a\") Then d.Remove \"a\"\n    ks = d.Keys\n"
        "    For Each k In p.Ages.Keys\n    Next\n    d.Clear\n"
        "    k = CStr(d.Count)\n";
    CHECK(check(inClick(decl, body)) == "");
    CHECK(check(inClick("", "    Dim d As Dictionary(Of Integer)\n    d(1) = 2\n")) != "");
}

TEST_CASE("a whole array member or Keys cannot stand for a single value")
{
    const std::string decl = "Type P\n    Lines() As String\nEnd Type\n";
    CHECK(check(inClick(decl, "    Dim p As P, s As String\n    s = p.Lines\n")) != "");
    CHECK(check(inClick(decl, "    Dim p As P, s As String\n    s = p.Lines(0)\n")) == "");
}

TEST_CASE("lexer: Of is reserved")
{
    CHECK(tf::isReservedKeyword("of"));
}

// -- String functions (Phase 3 step 6a) -----------------------------------------

TEST_CASE("string functions take their documented arguments, with or without $")
{
    const std::string body =
        "    Dim s As String, n As Integer, a() As String\n"
        "    s = Mid$(s, 1) & Mid(s, 1, 2) & Left$(s, 1) & Right(s, 1)\n"
        "    n = InStr(s, \"a\") + InStr(2, s, \"a\") + InStrRev(s, \"a\") + Len(s)\n"
        "    s = UCase(LCase$(Trim(s))) & Space$(2) & String$(2, \"x\") & Chr(65)\n"
        "    n = Asc(s) + LenB(s)\n    s = Replace(StrReverse(s), \"a\", \"b\")\n"
        "    a = Split(s, \",\")\n    s = Join(a)\n";
    CHECK(check(inClick("", body)) == "");
    CHECK(check(inClick("", "    Dim s As String\n    s = Mid$(s)\n")) != "");
}

// -- Byte (Phase 3 step 6b) -----------------------------------------------------

TEST_CASE("a Byte takes constants 0 to 255; Byte arithmetic is Integer")
{
    CHECK(check(inClick("", "    Dim x As Byte, n As Integer\n    x = 255\n    n = x + 1\n"
                            "    x = CByte(n)\n")) == "");
    CHECK(check(inClick("", "    Dim x As Byte\n    x = 256\n")) != "");
    CHECK(check(inClick("", "    Dim x As Byte\n    x = x + 1\n")) != "");
}

TEST_CASE("lexer: Byte is reserved")
{
    CHECK(tf::isReservedKeyword("byte"));
}

// -- Files (Phase 3 step 6b) ----------------------------------------------------

TEST_CASE("file statements are known by their shape, so their words stay free as names")
{
    const std::string decl = "Sub Append(s As String)\nEnd Sub\n";
    const std::string body = "    Dim Line As String, n As Integer\n"
                             "    Open \"a\" For Append As #1\n    Print #1, Line; n\n"
                             "    Close\n    Append Line\n    Open \"a\" For Input As 1\n"
                             "    Line Input #1, Line\n    Close #1\n    n = FreeFile\n";
    CHECK(check(inClick(decl, body)) == "");
}

TEST_CASE("Get and Put move bytes between a binary file and a Byte array")
{
    const std::string body = "    Dim buf() As Byte, fixed(3) As Byte, at As Long\n"
                             "    Open \"d\" For Binary As #1\n    Get #1, , buf, 16\n"
                             "    Get #1, at, fixed\n    Put #1, 0, buf\n    Seek #1, at\n"
                             "    at = Seek(1)\n    Close\n";
    CHECK(check(inClick("", body)) == "");
    CHECK(check(inClick("", "    Dim fixed(3) As Byte\n    Get #1, 0, fixed, 2\n")) != "");
}

TEST_CASE("file-system routines: statements, functions, and Name ... As by its shape")
{
    const std::string body = "    Dim s As String, Name As String, n As Long\n"
                             "    Name = \"x\"\n    MkDir Name\n    Name Name As \"y\"\n"
                             "    s = Dir$(\"*\") & Dir & CurDir$ & FileDateTime(s)\n"
                             "    n = FileLen(s)\n    If Exists(s) Then Kill s\n";
    CHECK(check(inClick("", body)) == "");
}

TEST_CASE("system functions, and Shell both as a function and as a statement")
{
    const std::string body = "    Dim s As String, n As Integer, secs As Double\n"
                             "    s = Command$ & Environ$(\"HOME\") & Now & Date$ & Time & CurDir\n"
                             "    n = Shell(\"true\")\n    Shell \"true\"\n    secs = Timer\n";
    CHECK(check(inClick("", body)) == "");
}

// -- Modules (Phase 3 step 7) ----------------------------------------------------

TEST_CASE("Public and Private mark module-level declarations")
{
    const std::string decl =
        "Public total As Long\nPrivate count As Integer\n"
        "Public Const LIMIT = 3\nPrivate Type P\n    X As Integer\nEnd Type\n"
        "Private Sub Helper()\nEnd Sub\nPublic Function Twice(n As Integer) As "
        "Integer\n    Twice = n * 2\nEnd Function\n";
    CHECK(check(inClick(decl, "    Dim p As P\n    Helper\n    total = Twice(LIMIT)\n")) == "");
    CHECK(check(kForm + "Public Dim x As Integer\n") != "");
}
