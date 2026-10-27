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
    Byte,    // 8-bit, 0 to 255
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

// Byte, Integer, Long and Double, in widening order.
inline bool isNumeric(Type t)
{
    return t == Type::Byte || t == Type::Integer || t == Type::Long || t == Type::Double;
}

// The wider of two numeric types: Byte < Integer < Long < Double.
inline Type widerNumeric(Type a, Type b)
{
    if (a == Type::Double || b == Type::Double)
    {
        return Type::Double;
    }
    if (a == Type::Long || b == Type::Long)
    {
        return Type::Long;
    }
    return a == Type::Integer || b == Type::Integer ? Type::Integer : Type::Byte;
}

// The type arithmetic is done in: Bytes are worked on as Integers (as in
// VB.NET), so b + 1 is an Integer and storing it back needs CByte.
inline Type arithmeticType(Type a, Type b)
{
    const Type t = widerNumeric(a, b);
    return t == Type::Byte ? Type::Integer : t;
}

// Built-in functions: conversions and number functions.
enum class Builtin
{
    None,
    CInt,
    CByte,
    CLng,
    CDbl,
    CStr,
    Str, // Str or Str$
    Val,
    Fix,
    Int,
    LBound,
    UBound,
    // String functions (Phase 3 step 6a); their signatures are in sema's
    // kStringFunctions.
    Len,
    LenB,
    Mid,
    Left,
    Right,
    InStr,
    InStrRev,
    LCase,
    UCase,
    LTrim,
    RTrim,
    Trim,
    Space,
    StringOf, // String$(n, s): String alone is the type keyword
    Chr,
    Asc,
    Replace,
    StrReverse,
    Split, // an array value, as d.Keys
    Join,
    // Files (step 6b): statements made into calls, and functions.
    FileOpen,
    FileClose,
    FileCloseAll,
    FilePrint,
    LineInput,
    Eof,
    Lof,
    FreeFile,
    SeekPos, // Seek(n)
    // A ListBox's methods and indexed properties (step 4.2): args[0] is it.
    ListAdd,
    ListRemove,
    ListClear,
    ListItem,
    ListSetItem,
    ListSelected,
    ListSetSelected,
    FileGet,
    FilePut,
    FileSeek, // Seek #n, offset
    // The file system (step 6b-4).
    FileKill,
    FileMkDir,
    FileRmDir,
    FileChDir,
    FileCopy,
    FileName, // Name old As new
    CurDir,
    FileLen,
    FileDateTime,
    Exists,
    Dir,
    DirNext, // Dir$() with no pattern
    // The system (step 6b-5).
    Command,
    Environ,
    Shell,
    Now,
    Date,
    Time,
    Timer,
    Count, // not a name programs use: For Each's element count of an array (a Long)
    // A Dictionary's members, made from d.Count, d.Exists(k), ... (args[0]
    // is the Dictionary, then the key): never written by name.
    DictCount,
    DictExists,
    DictKeys,
    DictItems,
    DictRemove,
    DictClear
};

// The library functions' signatures (strings, files), for checking and for
// C; statements such as Open are made into calls of these too. Each parameter
// S (a String), I (an Integer), L (a Long), Y (a Boolean), A (a whole String
// array), B (a whole Byte array with one dimension) or C (a ListBox); the first
// `required` must be given and the rest default to `defaults` (S: text, I:
// a number), except that InStr's optional parameter comes first
// (InStr([start,] s, find)). fails: a bad argument can raise an error.
struct StringFunction
{
    Builtin id;
    const char *params;
    int required;
    const char *defaults[3];
    Type result; // Split: an array of Strings
    const char *cName;
    bool fails;
};

inline constexpr StringFunction kStringFunctions[] = {
    {Builtin::Len, "S", 1, {}, Type::Integer, "tf_str_len", false},
    {Builtin::LenB, "S", 1, {}, Type::Integer, "tf_str_lenb", false},
    {Builtin::Mid, "SII", 2, {nullptr, nullptr, "2147483647"}, Type::String, "tf_str_mid", true},
    {Builtin::Left, "SI", 2, {}, Type::String, "tf_str_left", true},
    {Builtin::Right, "SI", 2, {}, Type::String, "tf_str_right", true},
    {Builtin::InStr, "ISS", 2, {"1"}, Type::Integer, "tf_str_instr", true},
    {Builtin::InStrRev, "SSI", 2, {nullptr, nullptr, "-1"}, Type::Integer, "tf_str_instrrev", true},
    {Builtin::LCase, "S", 1, {}, Type::String, "tf_str_lcase", false},
    {Builtin::UCase, "S", 1, {}, Type::String, "tf_str_ucase", false},
    {Builtin::LTrim, "S", 1, {}, Type::String, "tf_str_ltrim", false},
    {Builtin::RTrim, "S", 1, {}, Type::String, "tf_str_rtrim", false},
    {Builtin::Trim, "S", 1, {}, Type::String, "tf_str_trim", false},
    {Builtin::Space, "I", 1, {}, Type::String, "tf_str_space", true},
    {Builtin::StringOf, "IS", 2, {}, Type::String, "tf_str_repeat", true},
    {Builtin::Chr, "I", 1, {}, Type::String, "tf_str_chr", true},
    {Builtin::Asc, "S", 1, {}, Type::Integer, "tf_str_asc", true},
    {Builtin::Replace, "SSS", 3, {}, Type::String, "tf_str_replace", true}, // too long: 14
    {Builtin::StrReverse, "S", 1, {}, Type::String, "tf_str_reverse", false},
    {Builtin::Split, "SS", 1, {nullptr, " "}, Type::String, "tf_str_split", true},
    {Builtin::Join, "AS", 1, {nullptr, " "}, Type::String, "tf_str_join", true},
    {Builtin::FileOpen, "SII", 3, {}, Type::Void, "tf_file_open", true},
    {Builtin::FileClose, "I", 1, {}, Type::Void, "tf_file_close", true},
    {Builtin::FileCloseAll, "", 0, {}, Type::Void, "tf_file_close_all", true},
    {Builtin::FilePrint, "ISI", 3, {}, Type::Void, "tf_file_print", true},
    {Builtin::LineInput, "I", 1, {}, Type::String, "tf_file_line_input", true},
    {Builtin::Eof, "I", 1, {}, Type::Boolean, "tf_file_eof", true},
    {Builtin::Lof, "I", 1, {}, Type::Long, "tf_file_lof", true},
    {Builtin::FreeFile, "", 0, {}, Type::Integer, "tf_file_free", true},
    {Builtin::SeekPos, "I", 1, {}, Type::Long, "tf_file_seek_pos", true},
    // Get/Put #n, [offset], buffer [, count]: the parser gives a flag (1 if
    // written) before each optional part, so no value is a "missing" mark.
    {Builtin::ListAdd, "CSI", 2, {nullptr, nullptr, "-1"}, Type::Void, "tf_list_add", true},
    {Builtin::ListRemove, "CI", 2, {}, Type::Void, "tf_list_remove", true},
    {Builtin::ListClear, "C", 1, {}, Type::Void, "tf_list_clear", false},
    {Builtin::ListItem, "CI", 2, {}, Type::String, "tf_list_item", true},
    {Builtin::ListSetItem, "CIS", 3, {}, Type::Void, "tf_list_set_item", true},
    {Builtin::ListSelected, "CI", 2, {}, Type::Boolean, "tf_list_selected", true},
    {Builtin::ListSetSelected, "CIY", 3, {}, Type::Void, "tf_list_set_selected", true},
    {Builtin::FileGet, "IILBII", 6, {}, Type::Void, "tf_file_get", true},
    {Builtin::FilePut, "IILBII", 6, {}, Type::Void, "tf_file_put", true},
    {Builtin::FileSeek, "IL", 2, {}, Type::Void, "tf_file_seek", true},
    {Builtin::FileKill, "S", 1, {}, Type::Void, "tf_fs_kill", true},
    {Builtin::FileMkDir, "S", 1, {}, Type::Void, "tf_fs_mkdir", true},
    {Builtin::FileRmDir, "S", 1, {}, Type::Void, "tf_fs_rmdir", true},
    {Builtin::FileChDir, "S", 1, {}, Type::Void, "tf_fs_chdir", true},
    {Builtin::FileCopy, "SS", 2, {}, Type::Void, "tf_fs_copy", true},
    {Builtin::FileName, "SS", 2, {}, Type::Void, "tf_fs_name", true},
    {Builtin::CurDir, "", 0, {}, Type::String, "tf_fs_curdir", true},
    {Builtin::FileLen, "S", 1, {}, Type::Long, "tf_fs_len", true},
    {Builtin::FileDateTime, "S", 1, {}, Type::String, "tf_fs_datetime", true},
    {Builtin::Exists, "S", 1, {}, Type::Boolean, "tf_fs_exists", false},
    {Builtin::Dir, "S", 0, {}, Type::String, "tf_fs_dir", false},
    {Builtin::DirNext, "", 0, {}, Type::String, "tf_fs_dir_next", true},
    {Builtin::Command, "", 0, {}, Type::String, "tf_command", false},
    {Builtin::Environ, "S", 1, {}, Type::String, "tf_environ", false},
    {Builtin::Shell, "S", 1, {}, Type::Integer, "tf_shell", true},
    {Builtin::Now, "", 0, {}, Type::String, "tf_now", false},
    {Builtin::Date, "", 0, {}, Type::String, "tf_date", false},
    {Builtin::Time, "", 0, {}, Type::String, "tf_time", false},
    {Builtin::Timer, "", 0, {}, Type::Double, "tf_timer", false},
};

inline const StringFunction *findStringFunction(Builtin id)
{
    for (const StringFunction &f : kStringFunctions)
    {
        if (f.id == id)
        {
            return &f;
        }
    }
    return nullptr;
}

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
    bool isPrivate = false; // Private Type: its module only
    std::vector<std::unique_ptr<Variable>> members;
};

// Enum Name [As Integer | As Long] ... End Enum. A distinct type whose
// values are stored as its underlying type.
struct EnumDef
{
    std::string name;
    Location location;
    bool isPrivate = false; // Private Enum: its module only
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
    Member, // object.Property; inside a With, ".Property" has no object (left) until checked
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
    int rank = 0;        // a whole array (a Name or a record's array member): its dimensions
    bool linear = false; // an Index For Each makes: args[0] numbers the elements in storage order
    // An Index that is a Dictionary's entry: args[0] is the key. adding: the
    // target of d.Add (error 457 if the key is there).
    bool byKey = false;
    bool adding = false;
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
    With,     // With target ... End With: see Stmt::body
    Method,   // x.Name args: a Dictionary's Add, Remove or Clear (see Stmt::target)
    Library,  // Open, Close, Print #, ...: args are calls of built-ins, made in order
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
    // start. Return: value is the result, or null. Method: target is
    // x.Name, or x.Name(args) if written with parentheses, and args the
    // rest; the semantic pass makes Add an Assign and Remove and Clear a
    // value that is a call of their built-in.
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

    // For and Do bodies; the Try block of a Try. For a With, the semantic
    // pass puts first the assignments that evaluate its target once (into
    // hidden locals), then the body; target becomes the path each .Member
    // inside starts from.
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

    // For Each target In value: the semantic pass turns it into a loop with
    // a hidden counter. setup evaluates the array's path once and starts
    // the counter; condition tests it against the array's element count;
    // body begins by copying the element into target and counting it.
    bool isForEach = false;
    Block setup;

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
    bool isPublic = false; // module level: Public x / Public Const (else its module only)
    // Hidden locals a With keeps its target in. withCopy: a copy of a
    // Function's Type result, which (as the result itself) cannot be changed.
    bool isWithAnchor = false;
    bool withCopy = false;
    // Dictionary(Of T): type is T; held as a dynamic array of rank 1 (its
    // values) with an index of keys, so array rules mostly apply.
    bool isDictionary = false;
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
    std::vector<std::unique_ptr<ControlDef>> children; // a form's or a Frame's, in order
    bool isForm = false;
};

enum class EventKind
{
    None,
    Click,
    Change,
    DblClick,
    Timer,
    Load
};

struct Procedure
{
    std::string name;
    Location location;
    bool isPrivate = false; // Private Sub/Function: its module only (Public by default)
    Location endLocation;   // the End Sub / End Function line
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
    std::string name;                 // the file's name without .bas: Module.Name qualifies with it
    std::unique_ptr<ControlDef> form; // at most one in this version
    std::vector<std::unique_ptr<Variable>> globals;
    Block moduleStatements; // module-level Dim and Const statements, in order
    std::vector<std::unique_ptr<EnumDef>> enums;
    std::vector<std::unique_ptr<TypeDef>> types;
    std::vector<std::unique_ptr<Procedure>> procedures;
};

} // namespace tf::ast
