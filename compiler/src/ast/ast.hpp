// SPDX-License-Identifier: BSD-2-Clause
//
// ast.hpp -- the syntax tree produced by the parser and annotated by the
// semantic pass.
//
// Nodes are plain structs with a kind tag; ownership is by unique_ptr from
// parent to child. The semantic pass fills in the "resolved" fields (types,
// symbols) in place, and the code generator only reads a tree that passed
// semantic checks without errors.

#pragma once

#include "diag/diagnostics.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tf::ast
{

enum class Type
{
    Error, // set after a reported error, to avoid cascades
    Void,
    Integer, // 32-bit
    Long,    // 64-bit
    Double,
    String,
    Boolean,
    Control // a form or control used as an object
};

// Integer, Long and Double, in widening order.
inline bool isNumeric(Type t)
{
    return t == Type::Integer || t == Type::Long || t == Type::Double;
}

// The wider of two numeric types: Integer < Long < Double.
inline Type widerNumeric(Type a, Type b)
{
    if (a == Type::Double || b == Type::Double)
    {
        return Type::Double;
    }
    return a == Type::Long || b == Type::Long ? Type::Long : Type::Integer;
}

// Built-in functions: conversions and number functions.
enum class Builtin
{
    None,
    CInt,
    CLng,
    CDbl,
    CStr,
    Str, // Str or Str$
    Val,
    Fix,
    Int
};

const char *typeName(Type type);

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

enum class ExprKind
{
    IntegerLiteral, // Integer or Long, by value and suffix
    DoubleLiteral,
    StringLiteral,
    BooleanLiteral,
    Name,   // a variable, control or form
    Member, // object.Property
    Call,   // Function(args); a bare parameterless Function name becomes one too
    Unary,
    Binary
};

struct Variable; // defined below
struct ControlDef;
struct Procedure;

struct Expr
{
    ExprKind kind;
    Location location;
    Type type = Type::Error; // set by the semantic pass

    // IntegerLiteral / BooleanLiteral; DoubleLiteral
    std::int64_t integer = 0;
    double real = 0;
    // StringLiteral value, Name identifier, Member property name, operator text
    std::string text;
    char suffix = 0; // type suffix written on a Name, or 0

    std::unique_ptr<Expr> left;              // Unary operand, Binary left, Member object
    std::unique_ptr<Expr> right;             // Binary right
    std::vector<std::unique_ptr<Expr>> args; // Call arguments (text is the callee)

    // Resolved by the semantic pass.
    Variable *variable = nullptr;    // Name
    ControlDef *control = nullptr;   // Name
    Procedure *procedure = nullptr;  // Call of a Sub or Function
    Builtin builtin = Builtin::None; // Call of a built-in function
};

using ExprPtr = std::unique_ptr<Expr>;

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

enum class StmtKind
{
    Dim,
    Assign,
    If,
    Call,
    For,
    Do,     // Do/Loop in all its forms, and While ... Wend / End While
    Exit,   // Exit For / Do / While / Sub / Function
    Return, // Return [value]: leaves the Sub or Function
    End     // the End statement: terminate the program
};

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;
using Block = std::vector<StmtPtr>;

// One "ElseIf cond Then" (or the first "If cond Then") arm.
struct IfArm
{
    ExprPtr condition;
    Block body;
};

struct Stmt
{
    StmtKind kind;
    Location location;

    // Dim: the variables it declares (owned by the procedure or module).
    std::vector<Variable *> declared;
    bool insideLoop = false; // set by the semantic pass: re-initialised each pass

    // Assign: target = value. For: target is the loop variable, value the
    // start. Return: value is the result, or null.
    ExprPtr target;
    ExprPtr value;

    // If
    std::vector<IfArm> arms;
    Block elseBody;
    bool hasElse = false;

    // Call: callee(args)
    std::string callee;
    std::vector<ExprPtr> args;
    Procedure *procedure = nullptr; // resolved

    // For: For target = value To limit [Step step] ... Next [nextName]
    ExprPtr limit;
    ExprPtr step; // null when there is no Step
    std::string nextName;
    Location nextLocation;

    // Do: the loop condition, if any, tested before or after the body.
    // A While ... Wend loop is a Do with a test at the top and isWhile set.
    ExprPtr condition;
    bool testAtTop = false;
    bool until = false; // Until cond: leave when it is True
    bool isWhile = false;
    Location loopLocation; // the Loop / Wend / End While line

    // For and Do bodies.
    Block body;

    // Exit: which construct it leaves ("For", "Do", "While", "Sub",
    // "Function"), and for loops the loop statement, resolved.
    std::string exitWhat;
    const Stmt *exitLoop = nullptr;
};

// ---------------------------------------------------------------------------
// Declarations
// ---------------------------------------------------------------------------

struct Variable
{
    std::string name;
    Type type = Type::Error;
    Location location;
    bool isModuleLevel = false;
    bool isParameter = false;
    bool byRef = false;    // parameters only
    bool isResult = false; // the value a Function returns, named after it
};

// A property setting inside a Begin ... End block.
struct PropertySetting
{
    std::string name;
    Location location;
    ExprPtr value; // a literal
};

struct ControlDef
{
    std::string typeName; // "Form", "TextBox", "CommandButton"
    std::string name;
    Location location;
    std::vector<PropertySetting> properties;
    std::vector<std::unique_ptr<ControlDef>> children; // forms only
    bool isForm = false;
};

enum class EventKind
{
    None,
    Click,
    Change,
    Load
};

struct Procedure
{
    std::string name;
    Location location;
    Location endLocation; // the End Sub / End Function line
    bool isFunction = false;
    std::vector<std::unique_ptr<Variable>> parameters;
    std::unique_ptr<Variable> result; // Functions only; its type is the return type
    Block body;
    // Every Dim in the body, at any depth. Storage lasts for the whole call;
    // block scope is a compile-time rule only.
    std::vector<std::unique_ptr<Variable>> locals;

    // Event wiring, resolved by the semantic pass.
    ControlDef *eventControl = nullptr;
    EventKind event = EventKind::None;
};

struct Module
{
    std::string fileName;
    std::unique_ptr<ControlDef> form; // at most one in this version
    std::vector<std::unique_ptr<Variable>> globals;
    Block moduleStatements; // module-level Dim statements
    std::vector<std::unique_ptr<Procedure>> procedures;
};

} // namespace tf::ast
