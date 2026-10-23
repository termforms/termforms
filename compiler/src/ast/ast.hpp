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
    Integer,
    String,
    Boolean,
    Control // a form or control used as an object
};

const char *typeName(Type type);

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

enum class ExprKind
{
    IntegerLiteral,
    StringLiteral,
    BooleanLiteral,
    Name,   // a variable, control or form
    Member, // object.Property
    Unary,
    Binary
};

struct Variable; // defined below
struct ControlDef;

struct Expr
{
    ExprKind kind;
    Location location;
    Type type = Type::Error; // set by the semantic pass

    // IntegerLiteral / BooleanLiteral
    std::int64_t integer = 0;
    // StringLiteral value, Name identifier, Member property name, operator text
    std::string text;
    char suffix = 0; // type suffix written on a Name, or 0

    std::unique_ptr<Expr> left;  // Unary operand, Binary left, Member object
    std::unique_ptr<Expr> right; // Binary right

    // Resolved by the semantic pass (Name only).
    Variable *variable = nullptr;
    ControlDef *control = nullptr;
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
    End // the End statement: terminate the program
};

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;
using Block = std::vector<StmtPtr>;

struct Procedure;

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

    // Assign
    ExprPtr target;
    ExprPtr value;

    // If
    std::vector<IfArm> arms;
    Block elseBody;
    bool hasElse = false;

    // Call
    std::string callee;
    Procedure *procedure = nullptr; // resolved
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
    Location endLocation; // the End Sub line
    Block body;
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
