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
    Enum,   // a user Enum; which one is in the Expr's or Variable's enumDef
    Record, // a value of a user Type; which one is in typeDef
    Ref,    // a Ref to a user Type (typeDef), or Nothing (typeDef null)
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
    Int,
    LBound,
    UBound
};

// The most dimensions an array may have (TF_ARRAY_MAX_RANK in tf_runtime.h).
constexpr int kMaxArrayRank = 8;

struct Expr;

// One member of an Enum: "Red" or "Blue = 10".
struct EnumMember
{
    std::string name;
    Location location;
    std::unique_ptr<Expr> value; // null: one more than the member before
    std::int64_t resolved = 0;   // set by the semantic pass
    bool ready = false;          // resolved has been set (members are set in order)
};

struct Variable;

// Type Name ... End Type: a record type, a value like any other (copied on
// assignment). Its members are Variables with isMember set.
struct TypeDef
{
    std::string name;
    Location location;
    std::vector<std::unique_ptr<Variable>> members;
};

// Enum Name [As Integer | As Long] ... End Enum. A distinct type whose
// values are stored as its underlying type.
struct EnumDef
{
    std::string name;
    Location location;
    Type underlying = Type::Integer;
    std::vector<EnumMember> members;
};

const char *typeName(Type type);

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

enum class ExprKind
{
    IntegerLiteral, // Integer or Long, by value and suffix
    DoubleLiteral,
    Constant, // a Const or Enum member, replaced by its value by the semantic pass
    New,      // New Customer: text is the Type's name
    Nothing,  // the Ref that refers to nothing
    Index,    // a(i, j): an array element (parsed as a Call; the semantic pass decides)
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

    // IntegerLiteral / BooleanLiteral; DoubleLiteral. A Constant holds its
    // value in integer (Integer, Long, Boolean, Enum), real or text.
    std::int64_t integer = 0;
    double real = 0;
    // StringLiteral value, Name identifier, Member property name, operator text
    std::string text;
    char suffix = 0; // type suffix written on a Name, or 0

    std::unique_ptr<Expr> left;              // Unary operand, Binary left, Member object
    std::unique_ptr<Expr> right;             // Binary right
    std::vector<std::unique_ptr<Expr>> args; // Call arguments (text is the callee)

    // Resolved by the semantic pass.
    bool isErr = false;               // Name: the built-in Err object
    const EnumDef *enumDef = nullptr; // when type is Enum
    const TypeDef *typeDef = nullptr; // when type is Record
    const Variable *field = nullptr;  // Member of a record: the Type's member
    int rank = 0; // a whole array (a Name or a record's array member): its dimensions
    // CType(value, type): the target type, written as convertTo or, for an
    // Enum, by name. The semantic pass turns it into a CInt/CLng/... call.
    Type convertTo = Type::Error;
    std::string convertToName;
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
    Do,       // Do/Loop in all its forms, and While ... Wend / End While
    Exit,     // Exit For / Do / While / Sub / Function
    Return,   // Return [value]: leaves the Sub or Function
    Try,      // Try ... Catch [When c] ... Finally ... End Try
    Throw,    // Throw [number [, description]]; Err.Raise number [, source [, desc]]
    ErrClear, // Err.Clear
    Const,    // Const name [As type] = value, ...: declared holds the names
    ReDim,    // ReDim [Preserve] a(bounds), ...: args are a(bounds) as Call shapes
    Erase,    // Erase a, ...: args are the array names
    End       // the End statement: terminate the program
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

// One "Catch [When cond]" of a Try.
struct CatchClause
{
    Location location;
    ExprPtr when; // null: catches every error
    Block body;
};

struct Stmt
{
    StmtKind kind;
    Location location;

    // Dim and Const: the names they declare (owned by the procedure or
    // module). For a Const, constValues holds each one's value expression.
    std::vector<Variable *> declared;
    std::vector<ExprPtr> constValues;
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

    // For and Do bodies; the Try block of a Try.
    Block body;

    // Try: its Catch clauses in order, and Finally.
    std::vector<CatchClause> catches;
    Block finallyBody;
    bool hasFinally = false;
    Location finallyLocation;
    Location endLocation; // End Try

    // Throw: args are the number and description (none: rethrow). With
    // viaErrRaise they are Err.Raise's number, source and description.
    bool viaErrRaise = false;

    bool preserve = false; // ReDim Preserve

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
    std::string typeName;             // As <name> for an Enum, until resolved
    const EnumDef *enumDef = nullptr; // when type is Enum
    const TypeDef *typeDef = nullptr; // when type is Record
    Location location;
    bool isMember = false; // a member of a Type
    bool autoNew = false;  // Dim r As New Customer: a Ref given a new record at the Dim
    bool isConst = false;  // a Const: never stored, its uses become its value
    std::int64_t constInteger = 0;
    double constReal = 0;
    std::string constText;
    bool constReady = false; // its value has been worked out

    // Arrays: rank > 0. A fixed array (Dim a(10, 20)) has its upper bounds
    // as written (bounds) and worked out (upper); a dynamic one (Dim a(),
    // Dim a(,)) or an array parameter has neither until ReDim or the call.
    int rank = 0;
    bool isDynamic = false;
    std::vector<std::unique_ptr<Expr>> bounds;
    std::vector<std::int32_t> upper;
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
    Block moduleStatements; // module-level Dim and Const statements, in order
    std::vector<std::unique_ptr<EnumDef>> enums;
    std::vector<std::unique_ptr<TypeDef>> types;
    std::vector<std::unique_ptr<Procedure>> procedures;
};

} // namespace tf::ast
