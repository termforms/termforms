// SPDX-License-Identifier: BSD-2-Clause
//
// sema.cpp -- see sema.hpp.

#include "sema/sema.hpp"

#include "sema/catalog.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace tf
{

namespace
{

using namespace tf::ast;

// Built-in functions. Their names are case-sensitive like all names, and
// cannot be reused for variables or procedures.
struct BuiltinInfo
{
    const char *name;
    Builtin id;
    bool dollarForm; // may also be written with $ (Str$)
};

constexpr BuiltinInfo kBuiltins[] = {{"CInt", Builtin::CInt, false},
                                     {"CByte", Builtin::CByte, false},
                                     {"CLng", Builtin::CLng, false},
                                     {"CDbl", Builtin::CDbl, false},
                                     {"CStr", Builtin::CStr, false},
                                     {"Str", Builtin::Str, true},
                                     {"Val", Builtin::Val, false},
                                     {"Fix", Builtin::Fix, false},
                                     {"Int", Builtin::Int, false},
                                     {"LBound", Builtin::LBound, false},
                                     {"UBound", Builtin::UBound, false},
                                     {"Len", Builtin::Len, false},
                                     {"LenB", Builtin::LenB, false},
                                     {"Mid", Builtin::Mid, true},
                                     {"Left", Builtin::Left, true},
                                     {"Right", Builtin::Right, true},
                                     {"InStr", Builtin::InStr, false},
                                     {"InStrRev", Builtin::InStrRev, false},
                                     {"LCase", Builtin::LCase, true},
                                     {"UCase", Builtin::UCase, true},
                                     {"LTrim", Builtin::LTrim, true},
                                     {"RTrim", Builtin::RTrim, true},
                                     {"Trim", Builtin::Trim, true},
                                     {"Space", Builtin::Space, true},
                                     {"String", Builtin::StringOf, true},
                                     {"Chr", Builtin::Chr, true},
                                     {"Asc", Builtin::Asc, false},
                                     {"Replace", Builtin::Replace, false},
                                     {"StrReverse", Builtin::StrReverse, false},
                                     {"Split", Builtin::Split, false},
                                     {"Join", Builtin::Join, false},
                                     {"EOF", Builtin::Eof, false},
                                     {"LOF", Builtin::Lof, false},
                                     {"FreeFile", Builtin::FreeFile, false},
                                     {"Seek", Builtin::SeekPos, false},
                                     {"Kill", Builtin::FileKill, false},
                                     {"MkDir", Builtin::FileMkDir, false},
                                     {"RmDir", Builtin::FileRmDir, false},
                                     {"ChDir", Builtin::FileChDir, false},
                                     {"FileCopy", Builtin::FileCopy, false},
                                     {"CurDir", Builtin::CurDir, true},
                                     {"FileLen", Builtin::FileLen, false},
                                     {"FileDateTime", Builtin::FileDateTime, false},
                                     {"Exists", Builtin::Exists, false},
                                     {"Dir", Builtin::Dir, true},
                                     {"Command", Builtin::Command, true},
                                     {"Environ", Builtin::Environ, true},
                                     {"Shell", Builtin::Shell, false},
                                     {"Now", Builtin::Now, false},
                                     {"Date", Builtin::Date, true},
                                     {"Time", Builtin::Time, true},
                                     {"Timer", Builtin::Timer, false}};

const BuiltinInfo *findBuiltin(const std::string &name, char suffix)
{
    for (const BuiltinInfo &b : kBuiltins)
    {
        if (name == b.name && (suffix == 0 || (suffix == '$' && b.dollarForm)))
        {
            return &b;
        }
    }
    return nullptr;
}

// A type as the checks see it: an Enum type is only complete with its
// definition. Converts from a plain Type, so most code can ignore Enums.
struct TypeInfo
{
    Type type;
    const EnumDef *def = nullptr;
    const TypeDef *record = nullptr; // when type is Record

    TypeInfo(Type t, const EnumDef *d = nullptr, const TypeDef *r = nullptr)
        : type(t), def(d), record(r)
    {
    }

    bool byteConstant = false; // a constant 0 to 255 (see infoOf)

    bool operator==(const TypeInfo &other) const
    {
        return type == other.type && def == other.def && record == other.record;
    }
};

TypeInfo infoOf(const Expr &e)
{
    TypeInfo info(e.type, e.enumDef, e.typeDef);
    // A whole-number constant that fits may be stored in a Byte without
    // CByte (b = 65), as VB.NET's Option Strict allows.
    info.byteConstant = e.type == Type::Integer &&
                        (e.kind == ExprKind::IntegerLiteral || e.kind == ExprKind::Constant) &&
                        e.integer >= 0 && e.integer <= 255;
    return info;
}

TypeInfo infoOf(const Variable &v)
{
    return {v.type, v.enumDef, v.typeDef};
}

// An Enum value as a number: its underlying Integer or Long.
Type lowered(const TypeInfo &t)
{
    return t.type == Type::Enum && t.def != nullptr ? t.def->underlying : t.type;
}

class Checker
{
public:
    Checker(Module &module, Diagnostics &diagnostics) : module_(module), diagnostics_(diagnostics)
    {
    }

    bool run()
    {
        for (const BuiltinInfo &b : kBuiltins)
        {
            moduleNames_[b.name] = "built-in function";
        }
        moduleNames_["Err"] = "built-in object";
        registerEnums();
        registerTypes();
        registerControls();
        registerGlobals();
        registerProcedures();
        evaluateModuleConstants();
        resolveTypeMembers();           // their array bounds may use the module's Consts
        for (auto &v : module_.globals) // bounds may use the module's Consts
        {
            if (v->rank > 0 && !v->isDynamic && v->type != Type::Error)
            {
                resolveShape(*v);
            }
        }
        for (auto &proc : module_.procedures)
        {
            wireEvent(*proc);
        }
        for (auto &proc : module_.procedures)
        {
            checkProcedure(*proc);
        }
        checkEntryPoint();
        return !diagnostics_.hasErrors();
    }

private:
    // -- Declarations ------------------------------------------------------

    // Every name in module scope: controls, the form, globals and Subs.
    bool claimModuleName(const std::string &name, Location where, const char *what)
    {
        auto [it, inserted] = moduleNames_.emplace(name, what);
        if (!inserted)
        {
            diagnostics_.error(where, "'" + name + "' is already declared as a " + it->second);
            return false;
        }
        return true;
    }

    void registerControls()
    {
        ControlDef *form = module_.form.get();
        if (form == nullptr)
        {
            return;
        }
        claimModuleName(form->name, form->location, "form");
        controls_[form->name] = form;
        checkSettings(*form);

        for (auto &child : form->children)
        {
            if (catalog::findType(child->typeName) == nullptr || child->typeName == "Form")
            {
                diagnostics_.error(child->location, "unknown control type '" + child->typeName +
                                                        "' (this version has TextBox and "
                                                        "CommandButton)");
                continue;
            }
            if (claimModuleName(child->name, child->location, "control"))
            {
                controls_[child->name] = child.get();
            }
            checkSettings(*child);
        }
    }

    void checkSettings(const ControlDef &control)
    {
        const catalog::ControlType *type = catalog::findType(control.typeName);
        if (type == nullptr)
        {
            return;
        }
        std::set<std::string> seen;
        for (const PropertySetting &s : control.properties)
        {
            const catalog::Property *prop = catalog::findProperty(*type, s.name);
            if (prop == nullptr)
            {
                unknownProperty(*type, s.name, s.location);
                continue;
            }
            if (!seen.insert(s.name).second)
            {
                diagnostics_.error(s.location, "property '" + s.name + "' is set twice");
            }
            const Type valueType = s.value->kind == ExprKind::StringLiteral    ? Type::String
                                   : s.value->kind == ExprKind::BooleanLiteral ? Type::Boolean
                                   : s.value->kind == ExprKind::DoubleLiteral  ? Type::Double
                                                                               : Type::Integer;
            if (valueType != prop->type)
            {
                diagnostics_.error(s.location, "property '" + s.name + "' needs " +
                                                   article(prop->type) + ", not " +
                                                   article(valueType));
            }
            else if (valueType == Type::Integer && !fitsInteger(s.value->integer))
            {
                diagnostics_.error(s.location, "value does not fit in an Integer");
            }
        }
    }

    void unknownProperty(const catalog::ControlType &type, const std::string &name, Location where)
    {
        std::string message = type.name + std::string(" has no property '") + name + "'";
        if (const catalog::Property *near = catalog::findPropertyIgnoringCase(type, name))
        {
            message += " (did you mean '" + std::string(near->name) + "'?)";
        }
        diagnostics_.error(where, message);
    }

    void registerGlobals()
    {
        for (auto &v : module_.globals)
        {
            // A Const's type may still be unknown: its value decides it.
            if (v->isConst ? claimModuleName(v->name, v->location, "Const")
                           : v->type != Type::Error && resolveType(*v) &&
                                 claimModuleName(v->name, v->location, "variable"))
            {
                globals_[v->name] = v.get();
            }
        }
    }

    void registerProcedures()
    {
        for (auto &proc : module_.procedures)
        {
            if (!proc->name.empty() &&
                claimModuleName(proc->name, proc->location, proc->isFunction ? "Function" : "Sub"))
            {
                procedures_[proc->name] = proc.get();
            }
            for (auto &param : proc->parameters)
            {
                resolveType(*param);
            }
            if (proc->result != nullptr)
            {
                resolveType(*proc->result);
                if (proc->result->isDictionary)
                {
                    diagnostics_.error(proc->result->location,
                                       "a Function cannot return a Dictionary; pass one ByRef "
                                       "for it to fill");
                    proc->result->type = Type::Error;
                }
            }
        }
    }

    // -- Event wiring ------------------------------------------------------

    void wireEvent(Procedure &proc)
    {
        const std::size_t underscore = proc.name.rfind('_');
        if (underscore == std::string::npos || underscore == 0)
        {
            return;
        }
        const std::string objectName = proc.name.substr(0, underscore);
        const std::string eventName = proc.name.substr(underscore + 1);

        // Form events use "Form_", whatever the form is called (as in VB).
        ControlDef *control = nullptr;
        if (objectName == "Form" && module_.form != nullptr)
        {
            control = module_.form.get();
        }
        else if (auto it = controls_.find(objectName); it != controls_.end() && !it->second->isForm)
        {
            control = it->second;
        }

        if (control == nullptr)
        {
            // Same letters, different case: this Sub would never be called.
            for (const auto &[name, def] : controls_)
            {
                if (!def->isForm && catalog::equalsIgnoringCase(name, objectName))
                {
                    diagnostics_.warning(
                        proc.location, "Sub '" + proc.name + "' looks like an event handler for '" +
                                           name + "' but the case differs; it will not be called");
                    return;
                }
            }
            if (catalog::equalsIgnoringCase(objectName, "Form") && module_.form != nullptr)
            {
                diagnostics_.warning(proc.location, "Sub '" + proc.name +
                                                        "' looks like a form event handler; "
                                                        "write 'Form_' with that case");
            }
            return;
        }

        const catalog::ControlType *type = catalog::findType(control->typeName);
        if (type == nullptr)
        {
            return;
        }
        if (const catalog::Event *event = catalog::findEvent(*type, eventName))
        {
            // The runtime calls handlers with no arguments and ignores results.
            if (proc.isFunction)
            {
                diagnostics_.error(proc.location, "event handler '" + proc.name +
                                                      "' must be a Sub, not a Function");
            }
            else if (!proc.parameters.empty())
            {
                diagnostics_.error(proc.location,
                                   "event handler '" + proc.name + "' takes no parameters");
            }
            proc.eventControl = control;
            proc.event = event->kind;
            return;
        }
        if (const catalog::Event *near = catalog::findEventIgnoringCase(*type, eventName))
        {
            diagnostics_.warning(proc.location, "Sub '" + proc.name +
                                                    "' will not be called: the "
                                                    "event is '" +
                                                    near->name + "', not '" + eventName + "'");
            return;
        }
        diagnostics_.warning(proc.location, std::string(type->name) + " '" + control->name +
                                                "' has no event '" + eventName + "'; Sub '" +
                                                proc.name + "' will not be called");
    }

    // -- Procedure bodies --------------------------------------------------

    // Scope rules (VB.NET): parameters and the procedure's own Dims share
    // the outermost scope; each If arm and loop body opens a new one. A name
    // may be reused in sibling blocks but never hides one still in scope.
    void checkProcedure(Procedure &proc)
    {
        current_ = &proc;
        scopes_.assign(1, {});
        loops_.clear();
        catchDepth_ = 0;
        finallyFloors_.clear();
        withs_.clear();
        withCount_ = 0;
        for (auto &p : proc.parameters)
        {
            declareLocal(*p);
        }
        checkBlock(proc.body, false);
        current_ = nullptr;
    }

    void checkBlock(Block &block, bool newScope = true)
    {
        if (newScope)
        {
            scopes_.emplace_back();
        }
        for (auto &stmt : block)
        {
            if (stmt != nullptr)
            {
                checkStatement(*stmt);
            }
        }
        if (newScope)
        {
            scopes_.pop_back();
        }
    }

    void checkStatement(Stmt &s)
    {
        switch (s.kind)
        {
            case StmtKind::Dim:
                s.insideLoop = !loops_.empty();
                for (Variable *v : s.declared)
                {
                    declareLocal(*v);
                }
                break;
            case StmtKind::Assign:
                checkAssignment(s);
                break;
            case StmtKind::If:
                for (IfArm &arm : s.arms)
                {
                    requireBoolean(*arm.condition, "an If condition");
                    checkBlock(arm.body);
                }
                checkBlock(s.elseBody);
                break;
            case StmtKind::Call:
                checkCallStatement(s);
                break;
            case StmtKind::For:
                checkFor(s);
                break;
            case StmtKind::Do:
                if (s.condition != nullptr)
                {
                    requireBoolean(*s.condition, s.isWhile ? "a While condition"
                                                 : s.until ? "an Until condition"
                                                           : "a While condition");
                }
                loops_.push_back(&s);
                checkBlock(s.body);
                loops_.pop_back();
                break;
            case StmtKind::Exit:
                checkExit(s);
                break;
            case StmtKind::Return:
                checkReturn(s);
                break;
            case StmtKind::Try:
                checkTry(s);
                break;
            case StmtKind::Throw:
                checkThrow(s);
                break;
            case StmtKind::Const:
                checkLocalConst(s);
                break;
            case StmtKind::ReDim:
                checkReDim(s);
                break;
            case StmtKind::Erase:
                checkErase(s);
                break;
            case StmtKind::With:
                checkWith(s);
                break;
            case StmtKind::Method:
                checkMethod(s);
                break;
            case StmtKind::Library:
                for (auto &call : s.args)
                {
                    checkExpr(*call);
                }
                break;
            case StmtKind::ErrClear:
            case StmtKind::End:
                break;
        }
    }

    const char *procedureKind() const { return current_->isFunction ? "Function" : "Sub"; }

    void declareLocal(Variable &v)
    {
        if (v.type == Type::Error || !resolveType(v))
        {
            return; // already reported
        }
        if (v.rank > 0 && !v.isDynamic && !v.isParameter)
        {
            resolveShape(v);
            if (v.type == Type::Error)
            {
                return;
            }
        }
        if (const Variable *existing = lookupLocal(v.name))
        {
            diagnostics_.error(v.location, "'" + v.name + "' is already declared " +
                                               (existing->isParameter ? "as a parameter " : "") +
                                               "in this " + procedureKind());
            return;
        }
        if (current_->isFunction && v.name == current_->name)
        {
            diagnostics_.error(v.location, "'" + v.name +
                                               "' is the name of this Function; assign to it "
                                               "to set the result");
            return;
        }
        if (controls_.count(v.name) != 0)
        {
            diagnostics_.error(v.location, "'" + v.name + "' is the name of a control");
            return;
        }
        if (v.name == "Err")
        {
            diagnostics_.error(v.location, "'Err' is the built-in error object");
            return;
        }
        if (findBuiltin(v.name, 0) != nullptr)
        {
            diagnostics_.error(v.location, "'" + v.name + "' is the name of a built-in function");
            return;
        }
        scopes_.back()[v.name] = &v;
    }

    Variable *lookupLocal(const std::string &name) const
    {
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope)
        {
            if (auto it = scope->find(name); it != scope->end())
            {
                return it->second;
            }
        }
        return nullptr;
    }

    void requireBoolean(Expr &condition, const char *what)
    {
        const Type t = checkExpr(condition);
        if (t != Type::Boolean && t != Type::Error)
        {
            diagnostics_.error(condition.location,
                               std::string(what) + " must be Boolean, not " + typeName(t));
        }
    }

    void checkAssignment(Stmt &s)
    {
        Expr &target = *s.target;
        if (target.kind == ExprKind::Name)
        {
            const Variable *v = lookupVariable(target.text);
            if (v != nullptr && v->rank > 0)
            {
                checkArrayAssignment(s); // b = a
                return;
            }
            if (v != nullptr && v->isConst)
            {
                checkExpr(*s.value);
                diagnostics_.error(target.location,
                                   "'" + target.text + "' is a Const and cannot be assigned to");
                return;
            }
        }
        checkWhole(target); // o.Lines = other is a whole array
        if (target.kind == ExprKind::Member && target.field != nullptr && target.rank > 0)
        {
            checkArrayAssignment(s); // o.Lines = other
            return;
        }
        checkExpr(*s.value);

        if (target.kind == ExprKind::Name && target.control != nullptr)
        {
            diagnostics_.error(target.location, "cannot assign to the control '" + target.text +
                                                    "' itself; assign to one of its properties");
            return;
        }
        if (target.kind == ExprKind::Member && target.left->isErr)
        {
            diagnostics_.error(target.location, "Err." + target.text +
                                                    " is read-only; set it with Throw or "
                                                    "Err.Raise");
            return;
        }
        if (target.kind == ExprKind::Constant)
        {
            diagnostics_.error(target.location, "an Enum member cannot be assigned to");
            return;
        }
        if (target.kind == ExprKind::Member && target.field != nullptr && !isLvalue(target))
        {
            diagnostics_.error(target.location, "cannot assign to a member of a Function's "
                                                "result; store the result in a variable first");
            return;
        }
        if (target.kind == ExprKind::Index ||
            (target.kind == ExprKind::Member && target.field != nullptr))
        {
            if (!readOnlyPath(target, target.location))
            {
                requireAssignable(infoOf(target), infoOf(*s.value), s.value->location);
            }
            return;
        }
        if (target.kind == ExprKind::Name || target.kind == ExprKind::Member)
        {
            requireAssignable(infoOf(target), infoOf(*s.value), s.value->location);
            return;
        }
        if (target.kind == ExprKind::Call && target.procedure != nullptr)
        {
            diagnostics_.error(target.location, "cannot assign to Function '" + target.text +
                                                    "'; only its own body sets its result");
            return;
        }
        diagnostics_.error(target.location, "this cannot be assigned to");
    }

    // Option Strict: the same type, or a numeric widening. An Enum takes only
    // its own values, and widens to its underlying type and beyond.
    static bool assignable(const TypeInfo &target, const TypeInfo &value)
    {
        if (target.type == Type::Error || value.type == Type::Error)
        {
            return true;
        }
        if (target.type == Type::Enum)
        {
            return value.type == Type::Enum && value.def == target.def;
        }
        // Refs take Refs of their Type and Nothing; a Ref and a value of its
        // Type convert into each other by copying (settled 27 Oct 2026).
        if (target.type == Type::Ref)
        {
            return (value.type == Type::Ref &&
                    (value.record == target.record || value.record == nullptr)) ||
                   (value.type == Type::Record && value.record == target.record);
        }
        if (target.type == Type::Record)
        {
            return (value.type == Type::Record || value.type == Type::Ref) &&
                   value.record == target.record;
        }
        if (value.type == Type::Record || value.type == Type::Ref)
        {
            return false;
        }
        const Type v = lowered(value);
        if (target.type == v || (target.type == Type::Byte && value.byteConstant))
        {
            return true;
        }
        return isNumeric(target.type) && isNumeric(v) &&
               widerNumeric(target.type, v) == target.type;
    }

    void requireAssignable(const TypeInfo &target, const TypeInfo &value, Location where)
    {
        if (!assignable(target, value))
        {
            diagnostics_.error(where, std::string("cannot assign ") + article(value) + " to " +
                                          article(target) + " (Option Strict)" +
                                          conversionHint(target, value));
        }
    }

    // "; use CInt(value) to convert", or nothing when no conversion exists.
    static std::string conversionHint(const TypeInfo &targetInfo, const TypeInfo &valueInfo)
    {
        if (targetInfo.type == Type::Enum)
        {
            return isNumeric(lowered(valueInfo))
                       ? "; use CType(value, " + targetInfo.def->name + ") to convert"
                       : "";
        }
        const Type target = targetInfo.type;
        const Type value = lowered(valueInfo);
        const char *convert = target == Type::Byte      ? "CByte"
                              : target == Type::Integer ? "CInt"
                              : target == Type::Long    ? "CLng"
                              : target == Type::Double  ? "CDbl"
                              : target == Type::String  ? "CStr"
                                                        : nullptr;
        const bool convertible =
            (isNumeric(target) && (isNumeric(value) || value == Type::String)) ||
            (target == Type::String && (isNumeric(value) || value == Type::Boolean));
        if (convert == nullptr || !convertible)
        {
            return "";
        }
        return std::string("; use ") + convert + "(value) to convert";
    }

    // -- Calls -------------------------------------------------------------

    Procedure *findProcedure(const std::string &name)
    {
        auto it = procedures_.find(name);
        return it == procedures_.end() ? nullptr : it->second;
    }

    // "no Sub or Function named 'x'", with a hint when only the case differs
    // or when the name is something that cannot be called.
    void unknownCallee(const std::string &name, Location where)
    {
        if (lookupLocal(name) != nullptr || globals_.count(name) != 0)
        {
            diagnostics_.error(where, "'" + name + "' is a variable and cannot be called");
            return;
        }
        std::string message = "no Sub or Function named '" + name + "'";
        std::vector<std::string> known;
        for (const auto &[procName, proc] : procedures_)
        {
            known.push_back(procName);
        }
        for (const BuiltinInfo &b : kBuiltins)
        {
            known.emplace_back(b.name);
        }
        for (const std::string &k : known)
        {
            if (catalog::equalsIgnoringCase(k, name))
            {
                message += " (did you mean '" + k + "'? names are case-sensitive)";
                break;
            }
        }
        diagnostics_.error(where, message);
    }

    void checkCallStatement(Stmt &s)
    {
        if (s.callee.empty())
        {
            return;
        }
        s.procedure = findProcedure(s.callee);
        // Kill "x", MkDir d, FileCopy a, b: built-in statements, written as
        // calls of a Sub (a program's own Sub of that name comes first).
        if (const BuiltinInfo *b = s.procedure == nullptr ? findBuiltin(s.callee, 0) : nullptr)
        {
            // Shell may be a statement too, its exit code unused.
            if (const StringFunction *f = findStringFunction(b->id);
                f != nullptr && (f->result == Type::Void || f->id == Builtin::Shell))
            {
                auto call = std::make_unique<Expr>();
                call->kind = ExprKind::Call;
                call->builtin = b->id;
                call->text = b->name;
                call->location = s.location;
                call->args = std::move(s.args);
                s.args.clear();
                s.args.push_back(std::move(call));
                s.kind = StmtKind::Library;
                checkExpr(*s.args[0]);
                return;
            }
        }
        if (s.procedure == nullptr && findBuiltin(s.callee, 0) != nullptr)
        {
            diagnostics_.error(s.location, "'" + s.callee +
                                               "' is a built-in function; use its result, as "
                                               "in x = " +
                                               s.callee + "(...)");
            return;
        }
        if (s.procedure == nullptr)
        {
            unknownCallee(s.callee, s.location);
            for (auto &arg : s.args)
            {
                checkExpr(*arg);
            }
            return;
        }
        checkArguments(*s.procedure, s.args, s.location);
    }

    // Arity and types. ByVal arguments follow the assignment rules; a ByRef
    // argument must be a variable of exactly the parameter's type, because
    // the callee writes through it.
    void checkArguments(const Procedure &callee, std::vector<ExprPtr> &args, Location where)
    {
        const std::size_t want = callee.parameters.size();
        for (std::size_t i = 0; i < args.size(); ++i)
        {
            // An array parameter takes a whole array; anything else a value.
            const Variable *param = i < want ? callee.parameters[i].get() : nullptr;
            if (param != nullptr && param->rank > 0)
            {
                checkArrayArgument(callee, i, *param, *args[i]);
            }
            else
            {
                checkExpr(*args[i]);
            }
        }
        if (args.size() != want)
        {
            diagnostics_.error(where, "'" + callee.name + "' takes " + std::to_string(want) +
                                          (want == 1 ? " argument" : " arguments") + ", not " +
                                          std::to_string(args.size()));
            return;
        }
        for (std::size_t i = 0; i < want; ++i)
        {
            const Variable &param = *callee.parameters[i];
            const Expr &arg = *args[i];
            if (param.type == Type::Error || arg.type == Type::Error || param.rank > 0)
            {
                continue; // array arguments were checked above
            }
            const std::string which =
                "argument " + std::to_string(i + 1) + " of '" + callee.name + "'";
            if (param.byRef)
            {
                if (!isLvalue(arg))
                {
                    diagnostics_.error(arg.location, which + " is ByRef, so it must be a variable");
                }
                else if (readOnlyPath(arg, arg.location))
                {
                }
                else if (arg.type != param.type || arg.enumDef != param.enumDef ||
                         arg.typeDef != param.typeDef)
                {
                    diagnostics_.error(arg.location, which + " is ByRef " + name(infoOf(param)) +
                                                         ", so it must be " +
                                                         article(infoOf(param)) +
                                                         " variable, not " + article(infoOf(arg)));
                }
                continue;
            }
            if (!assignable(infoOf(param), infoOf(arg)))
            {
                diagnostics_.error(arg.location, which + " must be " + article(infoOf(param)) +
                                                     ", not " + article(infoOf(arg)) +
                                                     " (Option Strict)" +
                                                     conversionHint(infoOf(param), infoOf(arg)));
            }
        }
    }

    Type checkCallExpr(Expr &e)
    {
        // A call the parser made for a statement (Open, Print #, ...), or one
        // already checked once (a With path, a re-check).
        if (const StringFunction *f = findStringFunction(e.builtin))
        {
            return checkStringFunction(e, e.text, *f);
        }
        if (e.convertTo != Type::Error)
        {
            return checkCType(e);
        }
        if (current_ != nullptr)
        {
            if (Variable *v = lookupVariable(e.text); v != nullptr && v->rank > 0)
            {
                return checkIndex(e, *v);
            }
        }
        e.procedure = findProcedure(e.text);
        if (e.procedure == nullptr)
        {
            if (const BuiltinInfo *b = findBuiltin(e.text, e.suffix))
            {
                return checkBuiltin(e, *b);
            }
            if (e.suffix != 0 && findBuiltin(e.text, 0) != nullptr)
            {
                diagnostics_.error(e.location, "'" + e.text + "' is written without a '" +
                                                   std::string(1, e.suffix) + "' suffix");
                return Type::Error;
            }
            unknownCallee(e.text, e.location);
            for (auto &arg : e.args)
            {
                checkExpr(*arg);
            }
            return Type::Error;
        }
        if (!e.procedure->isFunction)
        {
            diagnostics_.error(e.location, "Sub '" + e.text +
                                               "' does not return a value; call it as a "
                                               "statement, or make it a Function");
            return Type::Error;
        }
        checkArguments(*e.procedure, e.args, e.location);
        e.enumDef = e.procedure->result->enumDef;
        e.typeDef = e.procedure->result->typeDef;
        return e.procedure->result->type;
    }

    // Conversions and number functions; each takes one argument.
    Type checkBuiltin(Expr &e, const BuiltinInfo &b)
    {
        if (b.id == Builtin::LBound || b.id == Builtin::UBound)
        {
            return checkBounds(e, b);
        }
        if (const StringFunction *f = findStringFunction(b.id))
        {
            if (f->result == Type::Void)
            {
                for (auto &arg : e.args)
                {
                    checkExpr(*arg);
                }
                diagnostics_.error(e.location, "'" + e.text +
                                                   "' is a statement, not a function: "
                                                   "write it on its own, as in " +
                                                   e.text + " ...");
                return Type::Error;
            }
            return checkStringFunction(e, b.name, *f);
        }
        e.builtin = b.id;
        const std::string name = e.text + (e.suffix != 0 ? std::string(1, e.suffix) : ""); // Str$
        for (auto &arg : e.args)
        {
            checkExpr(*arg);
        }
        if (e.args.size() != 1)
        {
            diagnostics_.error(e.location, "'" + name + "' takes 1 argument, not " +
                                               std::to_string(e.args.size()));
            return Type::Error;
        }
        const Type t = lowered(infoOf(*e.args[0])); // an Enum converts as its number
        if (t == Type::Error)
        {
            return Type::Error;
        }
        auto wrongArgument = [&](const char *needs) {
            diagnostics_.error(e.args[0]->location,
                               "'" + name + "' needs " + needs + ", not " + article(t));
            return Type::Error;
        };
        switch (b.id)
        {
            case Builtin::CInt:
            case Builtin::CByte:
            case Builtin::CLng:
            case Builtin::CDbl:
                if (!isNumeric(t) && t != Type::String)
                {
                    return wrongArgument("a number or a String");
                }
                return b.id == Builtin::CByte  ? Type::Byte
                       : b.id == Builtin::CInt ? Type::Integer
                       : b.id == Builtin::CLng ? Type::Long
                                               : Type::Double;
            case Builtin::CStr:
                if (!isNumeric(t) && t != Type::String && t != Type::Boolean)
                {
                    return wrongArgument("a number, a String or a Boolean");
                }
                return Type::String;
            case Builtin::Str:
                return isNumeric(t) ? Type::String : wrongArgument("a number");
            case Builtin::Val:
                return t == Type::String ? Type::Double : wrongArgument("a String");
            case Builtin::Fix:
            case Builtin::Int:
                return isNumeric(t) ? t : wrongArgument("a number");
            case Builtin::LBound: // handled by checkBounds
            case Builtin::UBound:
            default:             // the string functions: checkStringFunction
            case Builtin::Count: // made by For Each and Dictionary members, never by name
            case Builtin::DictCount:
            case Builtin::DictExists:
            case Builtin::DictKeys:
            case Builtin::DictItems:
            case Builtin::DictRemove:
            case Builtin::DictClear:
            case Builtin::None:
                break;
        }
        return Type::Error;
    }

    // -- Enums and constants -------------------------------------------------

    void registerEnums()
    {
        for (auto &def : module_.enums)
        {
            if (!def->name.empty() && claimModuleName(def->name, def->location, "Enum"))
            {
                enums_[def->name] = def.get();
            }
            std::set<std::string> seen;
            for (const EnumMember &m : def->members)
            {
                if (!seen.insert(m.name).second)
                {
                    diagnostics_.error(m.location, "Enum " + def->name +
                                                       " has two members named '" + m.name + "'");
                }
            }
        }
    }

    // Gives an "As <name>" variable its Enum or Type; false (and an error)
    // if there is no such type.
    bool resolveType(Variable &v)
    {
        if (v.type == Type::Ref && v.typeDef == nullptr && !v.typeName.empty())
        {
            if (auto it = types_.find(v.typeName); it != types_.end())
            {
                v.typeDef = it->second;
                return true;
            }
            if (enums_.count(v.typeName) != 0)
            {
                diagnostics_.error(v.location,
                                   "Ref works with a Type, and " + v.typeName + " is an Enum");
            }
            else
            {
                unknownType(v.typeName, v.location);
            }
            v.type = Type::Error;
            return false;
        }
        if (v.type != Type::Enum || v.enumDef != nullptr)
        {
            return true;
        }
        if (auto it = enums_.find(v.typeName); it != enums_.end())
        {
            v.enumDef = it->second;
            return true;
        }
        if (auto it = types_.find(v.typeName); it != types_.end())
        {
            v.type = Type::Record;
            v.typeDef = it->second;
            return true;
        }
        unknownType(v.typeName, v.location);
        v.type = Type::Error;
        return false;
    }

    void registerTypes()
    {
        for (auto &def : module_.types)
        {
            if (!def->name.empty() && claimModuleName(def->name, def->location, "Type"))
            {
                types_[def->name] = def.get();
            }
            std::set<std::string> seen;
            for (const auto &m : def->members)
            {
                if (!seen.insert(m->name).second)
                {
                    diagnostics_.error(m->location, "Type " + def->name +
                                                        " has two members named '" + m->name + "'");
                }
            }
        }
    }

    // Member types and fixed array sizes; then no Type may contain itself
    // by value (it would be infinitely large): through a Ref, or a dynamic
    // array, it may.
    void resolveTypeMembers()
    {
        for (auto &def : module_.types)
        {
            for (auto &m : def->members)
            {
                if (m->type != Type::Error && resolveType(*m) && m->rank > 0 && !m->isDynamic)
                {
                    resolveShape(*m);
                }
            }
        }
        for (auto &def : module_.types)
        {
            std::vector<const TypeDef *> path;
            if (containsByValue(*def, *def, path))
            {
                std::string chain = def->name;
                for (const TypeDef *t : path)
                {
                    chain += " -> " + t->name;
                }
                diagnostics_.error(def->location,
                                   "Type " + def->name + " contains itself (" + chain +
                                       "); use a Ref member, or a dynamic array, to point to "
                                       "another " +
                                       def->name);
            }
        }
    }

    static bool containsByValue(const TypeDef &outer, const TypeDef &inner,
                                std::vector<const TypeDef *> &path)
    {
        for (const auto &m : inner.members)
        {
            if (m->type != Type::Record || m->typeDef == nullptr || m->isDynamic)
            {
                continue;
            }
            path.push_back(m->typeDef);
            if (m->typeDef == &outer ||
                (path.size() < 64 && containsByValue(outer, *m->typeDef, path)))
            {
                return true;
            }
            path.pop_back();
        }
        return false;
    }

    void unknownType(const std::string &name, Location where)
    {
        std::string message = "unknown type '" + name + "'";
        for (const auto &[known, def] : enums_)
        {
            if (catalog::equalsIgnoringCase(known, name))
            {
                message += " (did you mean '" + known + "'? names are case-sensitive)";
            }
        }
        diagnostics_.error(where, message);
    }

    // Module-level Enums and Consts, in source order, so each may use the
    // ones before it.
    void evaluateModuleConstants()
    {
        std::vector<std::pair<Location, std::function<void()>>> items;
        for (auto &def : module_.enums)
        {
            EnumDef *d = def.get();
            items.emplace_back(d->location, [this, d] { evaluateEnum(*d); });
        }
        for (auto &stmt : module_.moduleStatements)
        {
            if (stmt != nullptr && stmt->kind == StmtKind::Const)
            {
                Stmt *s = stmt.get();
                items.emplace_back(s->location, [this, s] { evaluateConst(*s); });
            }
        }
        std::stable_sort(items.begin(), items.end(),
                         [](const auto &a, const auto &b) { return a.first.line < b.first.line; });
        for (auto &item : items)
        {
            item.second();
        }
    }

    void evaluateEnum(EnumDef &def)
    {
        const std::int64_t lo = def.underlying == Type::Long ? INT64_MIN : INT32_MIN;
        const std::int64_t hi = def.underlying == Type::Long ? INT64_MAX : INT32_MAX;
        std::int64_t next = 0;
        bool nextValid = true;
        for (EnumMember &m : def.members)
        {
            if (m.value != nullptr)
            {
                checkExpr(*m.value);
                const auto value = fold(*m.value);
                if (!value)
                {
                    notConstant(*m.value);
                }
                else if (value->type != Type::Integer && value->type != Type::Long &&
                         !(value->type == Type::Enum && value->def == &def))
                {
                    diagnostics_.error(m.value->location,
                                       "an Enum member's value must be a whole number, not " +
                                           article(TypeInfo{value->type, value->def}));
                }
                else if (value->integer < lo || value->integer > hi)
                {
                    diagnostics_.error(m.value->location,
                                       "value does not fit in the Enum's " +
                                           std::string(typeName(def.underlying)));
                }
                else
                {
                    next = value->integer;
                    nextValid = true;
                }
            }
            else if (!nextValid)
            {
                diagnostics_.error(m.location, "'" + m.name + "' would be one more than the " +
                                                   typeName(def.underlying) + " can hold");
            }
            m.resolved = next;
            m.ready = true;
            nextValid = next < hi;
            next = nextValid ? next + 1 : next;
        }
    }

    // Const NAME [As type] = value: works out each value now; the names are
    // then constants, never variables.
    void evaluateConst(Stmt &s)
    {
        for (std::size_t i = 0; i < s.declared.size() && i < s.constValues.size(); ++i)
        {
            Variable &v = *s.declared[i];
            Expr &valueExpr = *s.constValues[i];
            checkExpr(valueExpr);
            const auto value = fold(valueExpr);
            if (!value)
            {
                notConstant(valueExpr);
                v.type = Type::Error;
                continue;
            }
            if (v.type == Type::Error) // no As or suffix: the value decides
            {
                v.type = value->type;
                v.enumDef = value->def;
            }
            else if (!resolveType(v))
            {
                continue;
            }
            const TypeInfo target{v.type, v.enumDef};
            const TypeInfo got{value->type, value->def};
            if (!assignable(target, got))
            {
                diagnostics_.error(valueExpr.location, "cannot give " + article(target) +
                                                           " Const " + article(got) +
                                                           " value (Option Strict)");
                v.type = Type::Error;
                continue;
            }
            v.constInteger = value->integer;
            v.constReal = v.type == Type::Double && value->type != Type::Double
                              ? static_cast<double>(value->integer)
                              : value->real;
            v.constText = value->text;
            v.constReady = true;
        }
    }

    void notConstant(const Expr &e)
    {
        if (!foldReported_)
        {
            diagnostics_.error(e.location, "a constant's value must be known when compiling: use "
                                           "literals, other Consts, Enum members and operators");
        }
        foldReported_ = false;
    }

    // A value worked out at compile time.
    struct Folded
    {
        Type type = Type::Error;
        const EnumDef *def = nullptr;
        std::int64_t integer = 0; // Integer, Long, Boolean, Enum
        double real = 0;          // Double
        std::string text;         // String
    };

    // Evaluates a checked constant expression with the runtime's rules
    // (checked arithmetic, no Infinity), so a Const never differs from the
    // same expression run as code. Nothing means it is not constant; an
    // overflow is reported here (foldReported_) and also gives nothing.
    std::optional<Folded> fold(const Expr &e)
    {
        Folded f;
        f.type = e.type;
        f.def = e.enumDef;
        switch (e.kind)
        {
            case ExprKind::IntegerLiteral:
            case ExprKind::BooleanLiteral:
                f.integer = e.integer;
                return f;
            case ExprKind::DoubleLiteral:
                f.real = e.real;
                return f;
            case ExprKind::StringLiteral:
                f.text = e.text;
                return f;
            case ExprKind::Constant:
                f.integer = e.integer;
                f.real = e.real;
                f.text = e.text;
                return f;
            case ExprKind::Unary:
            {
                const auto x = fold(*e.left);
                if (!x)
                {
                    return std::nullopt;
                }
                if (e.text == "Not")
                {
                    f.integer = x->integer == 0 ? 1 : 0;
                    return f;
                }
                if (e.type == Type::Double)
                {
                    f.real = -asReal(*x);
                    return f;
                }
                return checkedInteger(e, x->integer == INT64_MIN ? 0 : -x->integer,
                                      x->integer == INT64_MIN, f);
            }
            case ExprKind::Binary:
                return foldBinary(e, f);
            default:
                return std::nullopt; // a variable, a call, a property: not constant
        }
    }

    std::optional<Folded> foldBinary(const Expr &e, Folded f)
    {
        const auto l = fold(*e.left);
        const auto r = l ? fold(*e.right) : std::nullopt;
        if (!l || !r)
        {
            return std::nullopt;
        }
        const std::string &op = e.text;
        if (op == "&" || (op == "+" && e.type == Type::String))
        {
            f.text = asText(*l) + asText(*r);
            return f;
        }
        if (op == "And" || op == "Or")
        {
            f.integer = op == "And" ? (l->integer & r->integer) : (l->integer | r->integer);
            return f;
        }
        if (e.type == Type::Boolean) // a comparison
        {
            int order = 0;
            if (l->type == Type::String)
            {
                order = l->text.compare(r->text);
            }
            else if (l->type == Type::Double || r->type == Type::Double)
            {
                order = asReal(*l) < asReal(*r) ? -1 : asReal(*l) > asReal(*r) ? 1 : 0;
            }
            else
            {
                order = l->integer < r->integer ? -1 : l->integer > r->integer ? 1 : 0;
            }
            f.integer = op == "="    ? order == 0
                        : op == "<>" ? order != 0
                        : op == "<"  ? order < 0
                        : op == ">"  ? order > 0
                        : op == "<=" ? order <= 0
                                     : order >= 0;
            return f;
        }
        if (e.type == Type::Double)
        {
            const double a = asReal(*l);
            const double b = asReal(*r);
            if ((op == "/" || op == "Mod") && b == 0.0)
            {
                return foldError(e, "division by zero in a constant");
            }
            const double v = op == "+"   ? a + b
                             : op == "-" ? a - b
                             : op == "*" ? a * b
                             : op == "/" ? a / b
                             : op == "^" ? std::pow(a, b)
                                         : std::fmod(a, b);
            if (std::isnan(v))
            {
                return foldError(e, "a negative number to a fractional power in a constant");
            }
            if (!std::isfinite(v))
            {
                return foldError(e, "this constant is too large for a Double");
            }
            f.real = v;
            return f;
        }
        // Integer and Long: exact 64-bit arithmetic with the runtime's
        // overflow tests, then the result type's range.
        const std::int64_t a = l->integer;
        const std::int64_t b = r->integer;
        if ((op == "\\" || op == "Mod") && b == 0)
        {
            return foldError(e, "division by zero in a constant");
        }
        std::int64_t v = 0;
        bool over = false;
        if (op == "+")
        {
            over = (b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b);
            v = over ? 0 : a + b;
        }
        else if (op == "-")
        {
            over = (b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b);
            v = over ? 0 : a - b;
        }
        else if (op == "*")
        {
            over = multiplyOverflows(a, b);
            v = over ? 0 : a * b;
        }
        else if (op == "\\")
        {
            over = a == INT64_MIN && b == -1;
            v = over ? 0 : a / b;
        }
        else // Mod: INT64_MIN Mod -1 is 0, and undefined in C
        {
            v = b == -1 ? 0 : a % b;
        }
        return checkedInteger(e, v, over, f);
    }

    static bool multiplyOverflows(std::int64_t a, std::int64_t b)
    {
        if (a > 0)
        {
            return (b > 0 && a > INT64_MAX / b) || (b < 0 && b < INT64_MIN / a);
        }
        if (a < 0)
        {
            return (b > 0 && a < INT64_MIN / b) || (b < 0 && b < INT64_MAX / a);
        }
        return false;
    }

    // Stores v as e's integer type, or reports the overflow.
    std::optional<Folded> checkedInteger(const Expr &e, std::int64_t v, bool overflowed, Folded f)
    {
        const bool isLong =
            e.type == Type::Long || (e.type == Type::Enum && e.enumDef->underlying == Type::Long);
        if (overflowed || (!isLong && (v < INT32_MIN || v > INT32_MAX)))
        {
            return foldError(e, std::string("this constant overflows ") +
                                    (isLong ? "a Long" : "an Integer"));
        }
        f.integer = v;
        return f;
    }

    std::optional<Folded> foldError(const Expr &e, const std::string &message)
    {
        diagnostics_.error(e.location, message);
        foldReported_ = true;
        return std::nullopt;
    }

    static double asReal(const Folded &f)
    {
        return f.type == Type::Double ? f.real : static_cast<double>(f.integer);
    }

    // A constant as & would turn it into text at run time.
    static std::string asText(const Folded &f)
    {
        switch (f.type)
        {
            case Type::String:
                return f.text;
            case Type::Boolean:
                return f.integer != 0 ? "True" : "False";
            case Type::Double:
                return doubleText(f.real);
            default:
                return std::to_string(f.integer);
        }
    }

    // The runtime's Double text (tf_str_from_f64): shortest digits that read
    // back exactly; exponent form from 1E15 and below 0.0001.
    static std::string doubleText(double value)
    {
        if (value == 0.0)
        {
            return "0";
        }
        char sci[40];
        for (int precision = 1; precision <= 17; ++precision)
        {
            std::snprintf(sci, sizeof sci, "%.*e", precision - 1, value);
            if (std::strtod(sci, nullptr) == value)
            {
                break;
            }
        }
        std::string out;
        const char *p = sci;
        if (*p == '-')
        {
            out += '-';
            ++p;
        }
        std::string digits;
        for (; *p != 'e' && *p != 'E'; ++p)
        {
            if (*p != '.')
            {
                digits += *p;
            }
        }
        const int exponent = static_cast<int>(std::strtol(p + 1, nullptr, 10));
        while (digits.size() > 1 && digits.back() == '0')
        {
            digits.pop_back();
        }
        if (exponent < -4 || exponent >= 15)
        {
            out += digits[0];
            if (digits.size() > 1)
            {
                out += "." + digits.substr(1);
            }
            // "E+05": a sign and at least two digits, as the runtime writes it.
            const std::string magnitude = std::to_string(exponent < 0 ? -exponent : exponent);
            return out + "E" + (exponent < 0 ? '-' : '+') + (magnitude.size() < 2 ? "0" : "") +
                   magnitude;
        }
        if (exponent >= 0)
        {
            for (int i = 0; i <= exponent || i < static_cast<int>(digits.size()); ++i)
            {
                if (i == exponent + 1)
                {
                    out += '.';
                }
                out +=
                    i < static_cast<int>(digits.size()) ? digits[static_cast<std::size_t>(i)] : '0';
            }
            return out;
        }
        return out + "0." + std::string(static_cast<std::size_t>(-exponent - 1), '0') + digits;
    }

    // A Name or Member that is a Const or an Enum member becomes its value.
    static Type makeConstant(Expr &e, const Variable &v)
    {
        e.kind = ExprKind::Constant;
        e.integer = v.constInteger;
        e.real = v.constReal;
        e.text = v.constText;
        e.enumDef = v.enumDef;
        e.left.reset();
        return v.type;
    }

    // Color.Red
    Type resolveEnumMember(Expr &e, const EnumDef &def)
    {
        for (const EnumMember &m : def.members)
        {
            if (m.name == e.text)
            {
                if (!m.ready)
                {
                    diagnostics_.error(e.location, "'" + def.name + "." + m.name +
                                                       "' is used before its value is set");
                    return Type::Error;
                }
                e.kind = ExprKind::Constant;
                e.integer = m.resolved;
                e.enumDef = &def;
                e.left.reset();
                return Type::Enum;
            }
        }
        std::string message = "Enum " + def.name + " has no member '" + e.text + "'";
        for (const EnumMember &m : def.members)
        {
            if (catalog::equalsIgnoringCase(m.name, e.text))
            {
                message += " (did you mean '" + m.name + "'? names are case-sensitive)";
            }
        }
        diagnostics_.error(e.location, message);
        return Type::Error;
    }

    // CType(value, type): to an Enum, CInt/CLng of the value given the Enum's
    // type; to a built-in type, the matching conversion function.
    Type checkCType(Expr &e)
    {
        Type target = e.convertTo;
        const EnumDef *def = nullptr;
        if (target == Type::Enum)
        {
            auto it = enums_.find(e.convertToName);
            if (it == enums_.end())
            {
                if (types_.count(e.convertToName) != 0)
                {
                    diagnostics_.error(e.location, "CType cannot convert to a Type");
                    return Type::Error;
                }
                unknownType(e.convertToName, e.location);
                return Type::Error;
            }
            def = it->second;
            target = def->underlying;
        }
        const Builtin conversion = target == Type::Integer  ? Builtin::CInt
                                   : target == Type::Long   ? Builtin::CLng
                                   : target == Type::Double ? Builtin::CDbl
                                   : target == Type::String ? Builtin::CStr
                                                            : Builtin::None;
        if (conversion == Builtin::None)
        {
            for (auto &arg : e.args)
            {
                checkExpr(*arg);
            }
            diagnostics_.error(e.location,
                               std::string("CType cannot convert to ") + typeName(e.convertTo));
            return Type::Error;
        }
        for (const BuiltinInfo &b : kBuiltins)
        {
            if (b.id == conversion)
            {
                const Type t = checkBuiltin(e, b);
                if (def == nullptr || t == Type::Error)
                {
                    return t;
                }
                e.enumDef = def;
                return Type::Enum;
            }
        }
        return Type::Error;
    }

    // Const inside a procedure: worked out, then in scope like a Dim.
    void checkLocalConst(Stmt &s)
    {
        evaluateConst(s);
        for (Variable *v : s.declared)
        {
            declareLocal(*v);
        }
    }

    // "Integer", "Color", "Customer".
    static std::string name(const TypeInfo &info)
    {
        if (info.type == Type::Enum && info.def != nullptr)
        {
            return info.def->name;
        }
        if (info.type == Type::Record && info.record != nullptr)
        {
            return info.record->name;
        }
        if (info.type == Type::Ref)
        {
            return info.record != nullptr ? "Ref " + info.record->name : "Nothing";
        }
        return typeName(info.type);
    }

    // -- Types ---------------------------------------------------------------

    // record.Member: its type, and its rank if it is an array member.
    Type resolveField(Expr &e, const TypeDef &def)
    {
        for (const auto &m : def.members)
        {
            if (m->name == e.text)
            {
                e.field = m.get();
                e.enumDef = m->enumDef;
                e.typeDef = m->typeDef;
                e.rank = m->rank;
                return m->type;
            }
        }
        std::string message = "Type " + def.name + " has no member '" + e.text + "'";
        for (const auto &m : def.members)
        {
            if (catalog::equalsIgnoringCase(m->name, e.text))
            {
                message += " (did you mean '" + m->name + "'? names are case-sensitive)";
            }
        }
        diagnostics_.error(e.location, message);
        return Type::Error;
    }

    // o.Lines(i): an element of an array member.
    Type checkMemberIndex(Expr &e)
    {
        if (e.left->kind == ExprKind::Member && e.left->text == "Exists" && e.left->left != nullptr)
        {
            if (const Variable *d = dictionaryOf(*e.left->left))
            {
                return checkExists(e, *d);
            }
            if (e.left->left->type == Type::Error && e.left->left->kind == ExprKind::Member)
            {
                return Type::Error; // already reported
            }
        }
        checkWhole(*e.left);
        if (e.left->type == Type::Error)
        {
            for (auto &index : e.args)
            {
                checkExpr(*index);
            }
            return Type::Error;
        }
        if (e.left->field == nullptr || e.left->rank == 0)
        {
            for (auto &index : e.args)
            {
                checkExpr(*index);
            }
            diagnostics_.error(e.location, "'" + e.left->text + "' is not an array");
            return Type::Error;
        }
        e.field = e.left->field; // marks it checked
        return checkIndexArgs(e, *e.left->field, e.left->text);
    }

    // The variable an lvalue path starts from: c in c.Home.City, a in
    // a(i).Name; null for a Function's result or anything else.
    static const Variable *rootVariable(const Expr &e)
    {
        switch (e.kind)
        {
            case ExprKind::Name:
                return e.variable;
            case ExprKind::Index:
                return e.left != nullptr ? rootVariable(*e.left) : e.variable;
            case ExprKind::Member:
                return e.field != nullptr ? rootVariable(*e.left) : nullptr;
            default:
                return nullptr;
        }
    }

    // Can e be stored to or passed ByRef: a variable, an element, or a member
    // of one (not a Function's result, a property or a constant)?
    static bool isLvalue(const Expr &e)
    {
        // A member of a record reached through a Ref is in a shared record,
        // wherever the Ref came from (even a Function's result).
        if (e.kind == ExprKind::Member && e.field != nullptr && e.left->type == Type::Ref)
        {
            return true;
        }
        if (e.kind == ExprKind::Member && e.field != nullptr)
        {
            return isLvalue(*e.left);
        }
        const Variable *root = rootVariable(e);
        return root != nullptr && !root->withCopy &&
               !(e.kind == ExprKind::Name && e.variable->isConst);
    }

    // A ByVal array parameter is read-only, and so is everything inside it.
    bool readOnlyPath(const Expr &e, Location where)
    {
        // Through a Ref the record is shared, not part of the array.
        for (const Expr *p = &e; p != nullptr;)
        {
            if (p->kind == ExprKind::Member && p->left != nullptr && p->left->type == Type::Ref)
            {
                return false;
            }
            p = p->kind == ExprKind::Member || (p->kind == ExprKind::Index && p->left != nullptr)
                    ? p->left.get()
                    : nullptr;
        }
        const Variable *root = rootVariable(e);
        if (root != nullptr && root->withCopy)
        {
            diagnostics_.error(where, "this is part of a Function's result, which cannot be "
                                      "changed; store the result in a variable first");
            return true;
        }
        return root != nullptr && root->rank > 0 && readOnlyArray(*root, where);
    }

    // -- Dictionaries ----------------------------------------------------------

    // The Dictionary that object is (d, o.Ages, or .Ages in a With), or null
    // (anything else, checked as it is; nothing reported for a non-Dictionary).
    const Variable *dictionaryOf(Expr &object)
    {
        if (object.kind == ExprKind::Name)
        {
            const Variable *v = lookupVariable(object.text);
            return v != nullptr && v->isDictionary ? wholeArray(object, "") : nullptr;
        }
        if (object.kind != ExprKind::Member)
        {
            return nullptr;
        }
        checkWhole(object);
        return object.field != nullptr && object.field->isDictionary ? object.field : nullptr;
    }

    // d.Keys, d.Items or Split(...), checked or not: an array value.
    bool isDictionaryList(Expr &e)
    {
        if (e.kind == ExprKind::Call &&
            (e.builtin == Builtin::DictKeys || e.builtin == Builtin::DictItems ||
             e.builtin == Builtin::Split ||
             (e.builtin == Builtin::None && e.text == "Split" && e.suffix == 0 &&
              lookupVariable("Split") == nullptr)))
        {
            return true;
        }
        return e.kind == ExprKind::Member && (e.text == "Keys" || e.text == "Items") &&
               e.left != nullptr && dictionaryOf(*e.left) != nullptr;
    }

    // d (a Dictionary) where an array is wanted: says what to do instead.
    bool dictionaryHere(const Variable *v, Location where, const char *instead)
    {
        if (v == nullptr || !v->isDictionary)
        {
            return false;
        }
        diagnostics_.error(where, "'" + v->name + "' is a Dictionary; " + instead);
        return true;
    }

    // e (object.Name) becomes a call of the built-in for that member.
    static void becomeBuiltin(Expr &e, Builtin id)
    {
        ExprPtr object = std::move(e.left);
        e.kind = ExprKind::Call;
        e.builtin = id;
        e.args.clear();
        e.args.push_back(std::move(object));
    }

    // e checked where a whole array (or Dictionary) may stand: an array
    // member (o.Lines) or d.Keys is then not an error.
    Type checkWhole(Expr &e)
    {
        wantWhole_ = true;
        const Type t = checkExpr(e);
        wantWhole_ = false;
        return t;
    }

    // A member that is a whole array or Dictionary, where a single value is
    // wanted ("s = o.Lines"), is an error.
    Type wholeWanted(const Expr &e, Type t, bool whole)
    {
        if (t == Type::Error || e.rank == 0 || whole)
        {
            return t;
        }
        if (e.field != nullptr && e.field->isDictionary)
        {
            diagnostics_.error(e.location,
                               "'" + e.text + "' is a Dictionary; use an entry, as in " + e.text +
                                   "(\"key\"), or a member such as " + e.text + ".Count");
        }
        else
        {
            diagnostics_.error(e.location, "'" + e.text + "' is an array; use an element, as in " +
                                               e.text + "(0)");
        }
        return Type::Error;
    }

    // d.Count, d.Keys, d.Items.
    Type dictionaryMember(Expr &e, const Variable &d, bool whole)
    {
        const std::string &m = e.text;
        if (m == "Count")
        {
            becomeBuiltin(e, Builtin::DictCount);
            return Type::Integer;
        }
        if ((m == "Keys" || m == "Items") && !whole)
        {
            diagnostics_.error(e.location, "'" + m +
                                               "' gives an array: assign it to a dynamic "
                                               "array, or go through it with For Each");
            return Type::Error;
        }
        if (m == "Keys" || m == "Items")
        {
            becomeBuiltin(e, m == "Keys" ? Builtin::DictKeys : Builtin::DictItems);
            e.rank = 1;
            e.enumDef = m == "Keys" ? nullptr : d.enumDef;
            e.typeDef = m == "Keys" ? nullptr : d.typeDef;
            return m == "Keys" ? Type::String : d.type;
        }
        if (m == "Exists")
        {
            diagnostics_.error(e.location, "Exists needs a key: " + d.name + ".Exists(key)");
        }
        else if (m == "Add" || m == "Remove" || m == "Clear")
        {
            diagnostics_.error(e.location, "'" + m + "' is a statement: write " + d.name + "." + m +
                                               (m == "Add"      ? " key, value"
                                                : m == "Remove" ? " key"
                                                                : "") +
                                               " on its own");
        }
        else
        {
            diagnostics_.error(e.location, "a Dictionary has no member '" + m +
                                               "'; it has Count, Keys, Items, Exists, Add, "
                                               "Remove and Clear");
        }
        return Type::Error;
    }

    // d.Exists(key): parsed as an Index of the member Exists.
    Type checkExists(Expr &e, const Variable &d)
    {
        for (auto &key : e.args)
        {
            checkExpr(*key);
        }
        if (e.args.size() != 1)
        {
            diagnostics_.error(e.location, "Exists takes one key: " + d.name + ".Exists(key)");
            return Type::Error;
        }
        if (!requireKey(*e.args[0]))
        {
            return Type::Error;
        }
        ExprPtr key = std::move(e.args[0]);
        ExprPtr object = std::move(e.left->left);
        e.kind = ExprKind::Call;
        e.builtin = Builtin::DictExists;
        e.left = nullptr;
        e.args.clear();
        e.args.push_back(std::move(object));
        e.args.push_back(std::move(key));
        return Type::Boolean;
    }

    bool requireKey(const Expr &key)
    {
        const Type t = lowered(infoOf(key));
        if (t != Type::String && t != Type::Error)
        {
            diagnostics_.error(key.location, "a Dictionary's key is a String, not " +
                                                 article(infoOf(key)) + "; use CStr");
        }
        return t == Type::String;
    }

    // d.Add key, value / d.Remove key / d.Clear (also with parentheses).
    void checkMethod(Stmt &s)
    {
        std::vector<ExprPtr> args;
        ExprPtr member = std::move(s.target);
        if (member->kind == ExprKind::Index) // d.Remove("x")
        {
            args = std::move(member->args);
            member = std::move(member->left);
        }
        for (auto &arg : s.args)
        {
            args.push_back(std::move(arg));
        }
        s.args.clear();
        if (member->kind != ExprKind::Member)
        {
            diagnostics_.error(member->location, "expected '=' after this");
            return;
        }
        const Variable *d = member->left != nullptr ? dictionaryOf(*member->left) : nullptr;
        if (d == nullptr)
        {
            for (auto &arg : args)
            {
                checkExpr(*arg);
            }
            if (member->left == nullptr || member->left->type != Type::Error)
            {
                diagnostics_.error(member->location, "'" + member->text +
                                                         "' is not a statement; to set it, write " +
                                                         "'= value' after it");
            }
            return;
        }
        const std::string &m = member->text;
        const std::size_t wanted = m == "Add" ? 2 : m == "Remove" ? 1 : 0;
        if (m != "Add" && m != "Remove" && m != "Clear")
        {
            diagnostics_.error(member->location, "a Dictionary's statements are Add, Remove and "
                                                 "Clear, not '" +
                                                     m + "'");
            return;
        }
        if (args.size() != wanted)
        {
            for (auto &arg : args)
            {
                checkExpr(*arg);
            }
            diagnostics_.error(member->location,
                               m + (m == "Add"
                                        ? " takes a key and a value: " + d->name + ".Add key, value"
                                    : m == "Remove" ? " takes a key: " + d->name + ".Remove key"
                                                    : " takes nothing: " + d->name + ".Clear"));
            return;
        }
        ExprPtr object = std::move(member->left);
        if (m == "Add")
        {
            // d.Add k, v is d(k) = v that must add: an ordinary assignment.
            auto entry = std::make_unique<Expr>();
            entry->location = member->location;
            entry->adding = true;
            entry->args.push_back(std::move(args[0]));
            if (object->kind == ExprKind::Name)
            {
                entry->kind = ExprKind::Call; // d(k), as parsed
                entry->text = object->text;
            }
            else
            {
                entry->kind = ExprKind::Index; // o.Ages(k)
                entry->left = std::move(object);
            }
            s.kind = StmtKind::Assign;
            s.target = std::move(entry);
            s.value = std::move(args[1]);
            checkAssignment(s);
            return;
        }
        if (readOnlyPath(*object, object->location))
        {
            return;
        }
        auto call = std::make_unique<Expr>();
        call->kind = ExprKind::Call;
        call->location = member->location;
        call->text = m;
        call->type = Type::Void;
        call->builtin = m == "Remove" ? Builtin::DictRemove : Builtin::DictClear;
        call->args.push_back(std::move(object));
        if (m == "Remove")
        {
            checkExpr(*args[0]);
            if (!requireKey(*args[0]))
            {
                return;
            }
            call->args.push_back(std::move(args[0]));
        }
        s.value = std::move(call);
    }

    // -- With ----------------------------------------------------------------

    // With target ... End With. As VB, the target is evaluated once, at the
    // With: the record a Ref refers to, a Function's result, and the indexes
    // of array elements are kept in hidden locals, which the assignments
    // put at the front of the body set. What is left of the target is a path
    // from those (and from variables, which are not copied) to the record;
    // each .Member inside is that path plus the member, so it is checked and
    // compiled like any other. Re-walking the path rather than holding the
    // element's address keeps a ReDim inside the With safe: the element is
    // looked up again (error 9 if it has gone).
    void checkWith(Stmt &s)
    {
        Expr &target = *s.target;
        const Type t = checkExpr(target);
        const bool record = t == Type::Record || (t == Type::Ref && target.typeDef != nullptr);
        const bool usable =
            (record && target.rank == 0) || (t == Type::Control && target.kind == ExprKind::Name);
        if (t == Type::Ref && target.typeDef == nullptr)
        {
            diagnostics_.error(target.location, "Nothing has no members");
        }
        else if (t != Type::Error && !usable)
        {
            diagnostics_.error(target.location, "With needs a Type, a Ref or a control, not " +
                                                    (target.rank > 0 ? std::string("an array")
                                                                     : article(infoOf(target))));
        }

        scopes_.emplace_back();
        Block setup;
        if (usable)
        {
            anchorWith(s.target, setup, s.location);
        }
        withs_.push_back(usable ? s.target.get() : nullptr);
        checkBlock(s.body, false);
        withs_.pop_back();
        scopes_.pop_back();

        for (auto &stmt : s.body)
        {
            setup.push_back(std::move(stmt));
        }
        s.body = std::move(setup);
    }

    // Splits a checked With target: the part evaluated once moves into an
    // assignment to a hidden local, and the local takes its place.
    void anchorWith(ExprPtr &target, Block &setup, Location where)
    {
        // The path from the target's tip down to where it starts.
        std::vector<ExprPtr *> spine{&target};
        for (;;)
        {
            Expr &e = **spine.back();
            if ((e.kind == ExprKind::Member && e.field != nullptr) ||
                (e.kind == ExprKind::Index && e.left != nullptr))
            {
                spine.push_back(&e.left);
                continue;
            }
            break;
        }
        // Everything up to the last Ref, Function call or New on the path
        // is evaluated once: after it, the path is inside one record.
        std::size_t start = spine.size();
        for (std::size_t i = 0; i < spine.size(); ++i)
        {
            const Expr &e = **spine[i];
            const bool anchor = e.kind == ExprKind::Name && e.variable != nullptr &&
                                e.variable->isWithAnchor; // an enclosing With's
            if (!anchor && ((e.type == Type::Ref && e.rank == 0) || e.kind == ExprKind::Call ||
                            e.kind == ExprKind::New))
            {
                start = i;
                break;
            }
        }
        if (start < spine.size())
        {
            ExprPtr &slot = *spine[start];
            Variable &held = hiddenLocal(*slot);
            held.withCopy = slot->type == Type::Record; // a Function's Type result
            hide(slot, held, setup, where);
        }
        // Then the indexes after it (from an a(i) that starts the path),
        // left to right; constants need no copy.
        for (std::size_t i = start; i-- > 0;)
        {
            Expr &e = **spine[i];
            if (e.kind != ExprKind::Index)
            {
                continue;
            }
            for (auto &index : e.args)
            {
                if (index->kind != ExprKind::IntegerLiteral && index->kind != ExprKind::Constant)
                {
                    hide(index, hiddenLocal(*index), setup, where);
                }
            }
        }
    }

    // For Each x In a: x takes a copy of each element in turn. Like a With,
    // the array's path is evaluated once; then a hidden counter n runs
    // while n < the array's element count, read afresh each pass, so a
    // ReDim inside the loop that shrinks the array ends it early rather
    // than reading past the end. The loop stays a For for Exit For.
    void checkForEach(Stmt &s)
    {
        const Type varType = checkExpr(*s.target);
        // For Each k In d.Keys: the keys (or values) are copied into a hidden
        // array at the start, so changing d inside the loop is safe.
        StmtPtr list;
        const Variable *array = nullptr;
        if (isDictionaryList(*s.value))
        {
            if (checkWhole(*s.value) != Type::Error)
            {
                Variable &held = hiddenLocal(*s.value);
                held.rank = 1;
                held.isDynamic = true;
                const Location at = s.value->location;
                list = assign(nameOf(held, at), std::move(s.value), s.location);
                s.value = nameOf(held, at);
                array = &held;
            }
        }
        else
        {
            array = wholeArray(*s.value, "For Each");
            if (dictionaryHere(array, s.value->location, "go through its .Keys or .Items"))
            {
                array = nullptr;
            }
        }
        const bool variable = s.target->kind == ExprKind::Name && s.target->variable != nullptr &&
                              varType != Type::Error;
        if (varType != Type::Error && !variable)
        {
            diagnostics_.error(s.target->location, "the For Each loop variable must be a variable");
        }
        const TypeInfo element = array != nullptr
                                     ? TypeInfo(array->type, array->enumDef, array->typeDef)
                                     : TypeInfo(Type::Error);
        const bool usable = variable && array != nullptr && array->type != Type::Error;
        if (usable && !assignable(infoOf(*s.target), element))
        {
            diagnostics_.error(s.target->location,
                               "the elements of '" + s.value->text + "' are " + plural(element) +
                                   ", which '" + s.target->text + "' (" +
                                   article(infoOf(*s.target)) + ") cannot hold" +
                                   conversionHint(infoOf(*s.target), element));
        }
        if (!s.nextName.empty() && s.nextName != s.target->text)
        {
            diagnostics_.error(s.nextLocation, "'Next " + s.nextName + "' does not match 'For " +
                                                   "Each " + s.target->text + "'");
        }
        loops_.push_back(&s);
        checkBlock(s.body);
        loops_.pop_back();
        if (!usable)
        {
            return;
        }

        scopes_.emplace_back(); // the hidden locals
        if (list != nullptr)
        {
            s.setup.push_back(std::move(list));
        }
        anchorWith(s.value, s.setup, s.location);
        const Expr &path = *s.value;
        Expr like;
        like.kind = ExprKind::IntegerLiteral;
        like.type = Type::Long;
        like.location = s.location;
        Variable &counter = hiddenLocal(like);
        scopes_.pop_back();

        auto zero = std::make_unique<Expr>();
        zero->kind = ExprKind::IntegerLiteral;
        zero->type = Type::Long;
        zero->location = s.location;
        s.setup.push_back(assign(nameOf(counter, s.location), std::move(zero), s.location));

        // n < Count(a)
        auto count = std::make_unique<Expr>();
        count->kind = ExprKind::Call;
        count->builtin = Builtin::Count;
        count->type = Type::Long;
        count->location = s.location;
        count->args.push_back(clonePath(path, s.location));
        s.condition = binary("<", Type::Boolean, nameOf(counter, s.location), std::move(count));
        s.testAtTop = true;

        // x = element n of a; n = n + 1
        auto item = std::make_unique<Expr>();
        item->kind = ExprKind::Index;
        item->linear = true;
        item->location = s.location;
        item->type = array->type;
        item->enumDef = array->enumDef;
        item->typeDef = array->typeDef;
        if (path.kind == ExprKind::Name)
        {
            item->variable = path.variable; // a(n)
        }
        else
        {
            item->left = clonePath(path, s.location); // o.Lines(n)
            item->field = path.field;
        }
        item->args.push_back(nameOf(counter, s.location));
        auto one = std::make_unique<Expr>();
        one->kind = ExprKind::IntegerLiteral;
        one->integer = 1;
        one->type = Type::Long;
        one->location = s.location;
        Block body;
        body.push_back(assign(clonePath(*s.target, s.location), std::move(item), s.location));
        body.push_back(assign(nameOf(counter, s.location),
                              binary("+", Type::Long, nameOf(counter, s.location), std::move(one)),
                              s.location));
        for (auto &stmt : s.body)
        {
            body.push_back(std::move(stmt));
        }
        s.body = std::move(body);
    }

    static StmtPtr assign(ExprPtr target, ExprPtr value, Location where)
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Assign;
        s->location = where;
        s->target = std::move(target);
        s->value = std::move(value);
        return s;
    }

    static ExprPtr binary(const char *op, Type type, ExprPtr left, ExprPtr right)
    {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Binary;
        e->text = op;
        e->type = type;
        e->location = left->location;
        e->left = std::move(left);
        e->right = std::move(right);
        return e;
    }

    // "Strings", "Integers", "Customers".
    static std::string plural(const TypeInfo &info) { return name(info) + "s"; }

    Variable &hiddenLocal(const Expr &like)
    {
        auto v = std::make_unique<Variable>();
        v->name = "_with" + std::to_string(++withCount_); // no BASIC name starts with _
        v->type = like.type;
        v->typeDef = like.typeDef;
        v->enumDef = like.enumDef;
        v->location = like.location;
        v->isWithAnchor = true;
        Variable &held = *v;
        scopes_.back()[held.name] = &held;
        current_->locals.push_back(std::move(v));
        return held;
    }

    // slot = held, after "held = <what slot was>" joins the setup.
    static void hide(ExprPtr &slot, Variable &held, Block &setup, Location where)
    {
        const Location at = slot->location;
        setup.push_back(assign(nameOf(held, at), std::move(slot), where));
        slot = nameOf(held, at);
    }

    static ExprPtr nameOf(Variable &v, Location where)
    {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Name;
        e->location = where;
        e->text = v.name;
        e->type = v.type;
        e->typeDef = v.typeDef;
        e->enumDef = v.enumDef;
        e->rank = v.rank;
        e->variable = &v;
        return e;
    }

    // A copy of a With's path (Names, Members, Indexes and constant
    // indexes, all checked) for one .Member.
    static ExprPtr clonePath(const Expr &e, Location where)
    {
        auto c = std::make_unique<Expr>();
        c->kind = e.kind;
        c->location = where;
        c->type = e.type;
        c->integer = e.integer;
        c->real = e.real;
        c->text = e.text;
        c->suffix = e.suffix;
        c->enumDef = e.enumDef;
        c->typeDef = e.typeDef;
        c->field = e.field;
        c->rank = e.rank;
        c->linear = e.linear;
        c->byKey = e.byKey;
        c->variable = e.variable;
        c->control = e.control;
        if (e.left != nullptr)
        {
            c->left = clonePath(*e.left, where);
        }
        for (const auto &arg : e.args)
        {
            c->args.push_back(clonePath(*arg, where));
        }
        return c;
    }

    static std::string describeObject(const Expr &e)
    {
        return e.kind == ExprKind::Name || e.kind == ExprKind::Member ? "'" + e.text + "'" : "this";
    }

    // -- Arrays ------------------------------------------------------------

    Variable *lookupVariable(const std::string &name)
    {
        if (Variable *v = current_ != nullptr ? lookupLocal(name) : nullptr)
        {
            return v;
        }
        auto g = globals_.find(name);
        return g == globals_.end() ? nullptr : g->second;
    }

    // A fixed array's upper bounds must be known when compiling (use
    // Dim a() and ReDim for a size known only when running).
    void resolveShape(Variable &v)
    {
        if (v.rank > kMaxArrayRank)
        {
            diagnostics_.error(v.location, "an array has at most " + std::to_string(kMaxArrayRank) +
                                               " dimensions");
            v.type = Type::Error;
            return;
        }
        for (auto &bound : v.bounds)
        {
            checkExpr(*bound);
            const auto value = fold(*bound);
            if (!value)
            {
                if (!foldReported_)
                {
                    diagnostics_.error(bound->location,
                                       "the size of a fixed array must be a constant; for a size "
                                       "known only when running, use Dim " +
                                           v.name + "() and ReDim " + v.name + "(n)");
                }
                foldReported_ = false;
                v.type = Type::Error;
                return;
            }
            const Type t = lowered(TypeInfo{value->type, value->def});
            if (t != Type::Integer && t != Type::Long)
            {
                diagnostics_.error(bound->location, "an array bound must be a whole number, not " +
                                                        article(TypeInfo{value->type, value->def}));
                v.type = Type::Error;
                return;
            }
            if (value->integer < 0 || value->integer > INT32_MAX)
            {
                diagnostics_.error(bound->location,
                                   "an array's upper bound must be from 0 to 2147483647 (arrays "
                                   "start at 0)");
                v.type = Type::Error;
                return;
            }
            v.upper.push_back(static_cast<std::int32_t>(value->integer));
        }
    }

    // A whole array where one is wanted (b = a, ReDim, Erase, UBound, an
    // array argument): an array variable, or a record's array member
    // (o.Lines). Returns its declaration (the variable or the member), or
    // null after an error.
    const Variable *wholeArray(Expr &e, const char *context)
    {
        if (e.kind == ExprKind::Name)
        {
            Variable *v = lookupVariable(e.text);
            if (v != nullptr && v->rank > 0)
            {
                e.variable = v;
                e.type = v->type;
                e.enumDef = v->enumDef;
                e.typeDef = v->typeDef;
                e.rank = v->rank;
                return v;
            }
        }
        const Type t = checkWhole(e); // reports an undeclared name, and so on
        if (e.kind == ExprKind::Member && e.field != nullptr && e.rank > 0)
        {
            return e.field;
        }
        if (t != Type::Error)
        {
            diagnostics_.error(e.location, std::string(context) + " needs an array here");
        }
        return nullptr;
    }

    // "the array 'a' is ByVal here": its elements and size cannot change.
    bool readOnlyArray(const Variable &v, Location where)
    {
        if (v.isParameter && !v.byRef)
        {
            diagnostics_.error(
                where, std::string(v.isDictionary ? "the Dictionary '" : "the array '") + v.name +
                           "' is a ByVal parameter, so it cannot be changed here; "
                           "make it ByRef to change it");
            return true;
        }
        return false;
    }

    static std::string dimensions(int rank)
    {
        return rank == 1 ? "1 dimension" : std::to_string(rank) + " dimensions";
    }

    // a(i, j): one whole-number index per dimension.
    Type checkIndex(Expr &e, Variable &v)
    {
        e.kind = ExprKind::Index;
        e.variable = &v;
        return checkIndexArgs(e, v, v.name);
    }

    // The indexes of a(i, j) or o.Lines(i, j), checked against the array's
    // declaration; the element's type.
    Type checkIndexArgs(Expr &e, const Variable &array, const std::string &arrayName)
    {
        e.enumDef = array.enumDef;
        e.typeDef = array.typeDef;
        if (array.isDictionary) // d(key)
        {
            e.byKey = true;
            for (auto &key : e.args)
            {
                checkExpr(*key);
            }
            if (e.args.size() != 1)
            {
                diagnostics_.error(e.location, "'" + arrayName +
                                                   "' is a Dictionary: it takes one key, as in " +
                                                   arrayName + "(\"key\")");
                return Type::Error;
            }
            const Type t = lowered(infoOf(*e.args[0]));
            if (t != Type::String && t != Type::Error)
            {
                diagnostics_.error(e.args[0]->location, "a Dictionary's key is a String, not " +
                                                            article(infoOf(*e.args[0])) +
                                                            "; use CStr");
                return Type::Error;
            }
            return t == Type::Error ? Type::Error : array.type;
        }
        bool ok = true;
        for (auto &index : e.args)
        {
            checkExpr(*index);
            const Type t = lowered(infoOf(*index));
            if (t != Type::Integer && t != Type::Long && t != Type::Error)
            {
                diagnostics_.error(index->location, "an array index must be a whole number, not " +
                                                        article(infoOf(*index)) +
                                                        "; use CInt or CLng");
                ok = false;
            }
        }
        if (static_cast<int>(e.args.size()) != array.rank)
        {
            diagnostics_.error(e.location, "'" + arrayName + "' has " + dimensions(array.rank) +
                                               ", so it needs " + std::to_string(array.rank) +
                                               (array.rank == 1 ? " index" : " indexes") +
                                               ", not " + std::to_string(e.args.size()));
            ok = false;
        }
        return ok ? array.type : Type::Error;
    }

    // b = a: copies the elements; b must be dynamic (or an array parameter
    // passed ByRef) and of the same element type and dimensions.
    void checkArrayAssignment(Stmt &s)
    {
        const Variable *target = wholeArray(*s.target, "assignment");
        if (isDictionaryList(*s.value)) // keys = d.Keys, values = d.Items
        {
            checkWhole(*s.value);
            if (target == nullptr || s.value->type == Type::Error ||
                readOnlyPath(*s.target, s.target->location))
            {
                return;
            }
            const TypeInfo got = infoOf(*s.value);
            if (target->isDictionary || !target->isDynamic || target->rank != 1 ||
                !(infoOf(*target) == got))
            {
                diagnostics_.error(s.target->location,
                                   "'" + s.value->text + "' gives an array of " + name(got) +
                                       ", so it is assigned to a dynamic array: Dim " +
                                       target->name + "() As " + name(got));
            }
            return;
        }
        const Variable *source = wholeArray(*s.value, "assigning to an array");
        if (target == nullptr || source == nullptr || readOnlyPath(*s.target, s.target->location))
        {
            return;
        }
        if (target->isDictionary != source->isDictionary)
        {
            diagnostics_.error(s.value->location,
                               std::string("cannot assign ") + (source->isDictionary
                                                                    ? "a Dictionary to an array"
                                                                    : "an array to a Dictionary"));
            return;
        }
        if (target->isDictionary)
        {
            if (!(infoOf(*target) == infoOf(*source)))
            {
                diagnostics_.error(s.value->location,
                                   "cannot assign a Dictionary(Of " + name(infoOf(*source)) +
                                       ") to a Dictionary(Of " + name(infoOf(*target)) + ")");
            }
            return;
        }
        if (!target->isDynamic)
        {
            diagnostics_.error(
                s.target->location,
                "'" + target->name + "' is a fixed array and cannot be assigned to; declare it " +
                    (target->isMember ? "" : "Dim ") + target->name + "() to copy arrays into it");
            return;
        }
        if (source->type != target->type || source->enumDef != target->enumDef ||
            source->typeDef != target->typeDef || source->rank != target->rank)
        {
            diagnostics_.error(s.value->location,
                               "cannot assign an array of " + name(infoOf(*source)) + " with " +
                                   dimensions(source->rank) + " to an array of " +
                                   name(infoOf(*target)) + " with " + dimensions(target->rank));
        }
    }

    // ReDim [Preserve] a(n, m), o.Lines(n), ...: each target was parsed as
    // a Call (a(n)) or a member Index (o.Lines(n)); its arguments are bounds.
    void checkReDim(Stmt &s)
    {
        for (auto &target : s.args)
        {
            for (auto &bound : target->args)
            {
                checkExpr(*bound);
                const Type t = lowered(infoOf(*bound));
                if (t != Type::Integer && t != Type::Error)
                {
                    diagnostics_.error(
                        bound->location,
                        "an array bound must be an Integer, not " + article(infoOf(*bound)) +
                            (t == Type::Long || t == Type::Double ? "; use CInt" : ""));
                }
            }
            const Variable *v = nullptr;
            std::string arrayName = target->text;
            if (target->kind == ExprKind::Call)
            {
                Variable *found = lookupVariable(target->text);
                if (found == nullptr || found->rank == 0)
                {
                    diagnostics_.error(target->location,
                                       "ReDim needs an array, declared with Dim " + target->text +
                                           "()");
                    continue;
                }
                target->variable = found;
                v = found;
                if (dictionaryHere(v, target->location, "it grows as keys are added"))
                {
                    continue;
                }
            }
            else if (target->kind == ExprKind::Index && target->left != nullptr)
            {
                v = wholeArray(*target->left, "ReDim");
                arrayName = target->left->text;
                if (v == nullptr ||
                    dictionaryHere(v, target->location, "it grows as keys are added"))
                {
                    continue;
                }
            }
            else
            {
                diagnostics_.error(target->location, "ReDim needs an array and its new bounds");
                continue;
            }
            if ((target->left != nullptr ? readOnlyPath(*target->left, target->location)
                                         : readOnlyArray(*v, target->location)))
            {
                continue;
            }
            if (!v->isDynamic)
            {
                diagnostics_.error(target->location,
                                   "'" + arrayName +
                                       "' is a fixed array; only a dynamic array (Dim " +
                                       arrayName + "()) can be ReDim'd");
                continue;
            }
            if (static_cast<int>(target->args.size()) != v->rank)
            {
                diagnostics_.error(target->location,
                                   "'" + arrayName + "' has " + dimensions(v->rank) +
                                       ", so ReDim needs " + std::to_string(v->rank) +
                                       (v->rank == 1 ? " bound" : " bounds") + ", not " +
                                       std::to_string(target->args.size()));
            }
        }
    }

    void checkErase(Stmt &s)
    {
        for (auto &array : s.args)
        {
            const Variable *v = wholeArray(*array, "Erase");
            if (v != nullptr && !dictionaryHere(v, array->location, "empty it with .Clear"))
            {
                readOnlyPath(*array, array->location);
            }
        }
    }

    // Len, Mid$, InStr, Split, ...: arguments by the signature; missing
    // optional ones become their defaults, so codegen sees every argument.
    Type checkStringFunction(Expr &e, const std::string &baseName, const StringFunction &given_)
    {
        // Dir$() (or Dir$ alone) with no pattern goes on with the last one.
        const StringFunction &f = given_.id == Builtin::Dir && e.args.empty()
                                      ? *findStringFunction(Builtin::DirNext)
                                      : given_;
        const bool whole = wholeNow_; // Split gives an array: where is it used?
        e.builtin = f.id;
        const std::string name = baseName + (e.suffix != 0 ? std::string(1, e.suffix) : "");
        const int most = static_cast<int>(std::strlen(f.params));
        const int given = static_cast<int>(e.args.size());
        if (given < f.required || given > most)
        {
            for (auto &arg : e.args)
            {
                checkExpr(*arg);
            }
            diagnostics_.error(
                e.location,
                "'" + name + "' takes " +
                    (f.required == most
                         ? std::to_string(most)
                         : std::to_string(f.required) + (most == f.required + 1 ? " or " : " to ") +
                               std::to_string(most)) +
                    (most == 1 ? " argument" : " arguments") + ", not " + std::to_string(given));
            return Type::Error;
        }
        // Print #n, 5: a lone item that is not text becomes text, as & would
        // make it ("" & 5).
        if (f.id == Builtin::FilePrint && given == most)
        {
            ExprPtr &item = e.args[1];
            const Type t = checkExpr(*item);
            if (t != Type::String && t != Type::Error)
            {
                auto empty = std::make_unique<Expr>();
                empty->kind = ExprKind::StringLiteral;
                empty->location = item->location;
                auto joined = std::make_unique<Expr>();
                joined->kind = ExprKind::Binary;
                joined->text = "&";
                joined->location = item->location;
                joined->left = std::move(empty);
                joined->right = std::move(item);
                item = std::move(joined);
            }
        }
        // InStr(s, find) is InStr(1, s, find).
        const int skip = f.id == Builtin::InStr && given < most ? most - given : 0;
        bool ok = true;
        for (int i = 0; i < given; ++i)
        {
            Expr &arg = *e.args[static_cast<std::size_t>(i)];
            const char want = f.params[i + skip];
            if (want == 'A' || want == 'B')
            {
                const Type element = want == 'A' ? Type::String : Type::Byte;
                const Variable *a = wholeArray(arg, ("'" + name + "'").c_str());
                if (a != nullptr && (a->isDictionary || a->type != element || a->rank != 1))
                {
                    diagnostics_.error(arg.location, "'" + name + "' needs an array of " +
                                                         typeName(element) + "s with 1 dimension");
                    ok = false;
                }
                else if (a != nullptr && f.id == Builtin::FileGet)
                {
                    // Get fills (and with a count resizes) its buffer.
                    if (readOnlyPath(arg, arg.location))
                    {
                        ok = false;
                    }
                    else if (!a->isDynamic && e.args[4]->integer != 0)
                    {
                        diagnostics_.error(arg.location,
                                           "a Get with a count resizes its buffer, so '" + a->name +
                                               "' must be a dynamic array: Dim " + a->name +
                                               "() As Byte");
                        ok = false;
                    }
                }
                ok = ok && a != nullptr;
                continue;
            }
            checkExpr(arg);
            const TypeInfo got = infoOf(arg);
            const TypeInfo needed(want == 'S'   ? Type::String
                                  : want == 'L' ? Type::Long
                                                : Type::Integer);
            if (got.type != Type::Error && !assignable(needed, got))
            {
                diagnostics_.error(arg.location, argumentName(f.id, i + skip) + " of '" + name +
                                                     "' must be " + article(needed) + ", not " +
                                                     article(got) + conversionHint(needed, got));
                ok = false;
            }
            ok = ok && got.type != Type::Error;
        }
        // The defaults of the arguments left out.
        for (int i = 0; i < most - given; ++i)
        {
            const int at = skip > 0 ? i : given + i;
            auto value = std::make_unique<Expr>();
            value->location = e.location;
            if (f.params[at] == 'S')
            {
                value->kind = ExprKind::StringLiteral;
                value->type = Type::String;
                value->text = f.defaults[at];
            }
            else
            {
                value->kind = ExprKind::IntegerLiteral;
                value->type = Type::Integer;
                value->integer = std::strtoll(f.defaults[at], nullptr, 10);
            }
            e.args.insert(e.args.begin() + at, std::move(value));
        }
        if (!ok)
        {
            return Type::Error;
        }
        if (f.id == Builtin::Split)
        {
            if (!whole)
            {
                diagnostics_.error(e.location, "'Split' gives an array: assign it to a dynamic "
                                               "array of Strings, or go through it with For Each");
                return Type::Error;
            }
            e.rank = 1;
        }
        return f.result;
    }

    // "argument 2"; for a file statement, what the part is (its call also
    // carries flags the programmer did not write, so numbers would mislead).
    static std::string argumentName(Builtin id, int i)
    {
        switch (id)
        {
            case Builtin::FileOpen:
                return i == 0 ? "the file name" : "the file number";
            case Builtin::FileGet:
            case Builtin::FilePut:
                return i == 0 ? "the file number" : i == 2 ? "the offset" : "the count";
            case Builtin::FileSeek:
                return i == 0 ? "the file number" : "the offset";
            case Builtin::FileClose:
            case Builtin::FilePrint:
            case Builtin::LineInput:
                return i == 0 ? "the file number" : "argument " + std::to_string(i + 1);
            default:
                return "argument " + std::to_string(i + 1);
        }
    }

    // UBound(a [, dimension]) and LBound: an Integer.
    Type checkBounds(Expr &e, const BuiltinInfo &b)
    {
        e.builtin = b.id;
        if (e.args.empty() || e.args.size() > 2)
        {
            for (auto &arg : e.args)
            {
                checkExpr(*arg);
            }
            diagnostics_.error(e.location, std::string("'") + b.name +
                                               "' takes an array and, optionally, a dimension");
            return Type::Error;
        }
        const Variable *v = wholeArray(*e.args[0], b.name);
        if (dictionaryHere(v, e.args[0]->location, "use its .Count"))
        {
            return Type::Error;
        }
        if (e.args.size() == 2)
        {
            checkExpr(*e.args[1]);
            if (lowered(infoOf(*e.args[1])) != Type::Integer)
            {
                diagnostics_.error(e.args[1]->location, "the dimension must be an Integer, not " +
                                                            article(infoOf(*e.args[1])));
                return Type::Error;
            }
            const auto d = fold(*e.args[1]);
            foldReported_ = false;
            if (v != nullptr && d && (d->integer < 1 || d->integer > v->rank))
            {
                diagnostics_.error(e.args[1]->location,
                                   "'" + v->name + "' has " + dimensions(v->rank) +
                                       ": the dimension must be 1 to " + std::to_string(v->rank));
                return Type::Error;
            }
        }
        return v != nullptr ? Type::Integer : Type::Error;
    }

    // An array argument: a whole array of the parameter's element type and
    // dimensions. ByRef lets the callee change or ReDim it, so a ByVal
    // array parameter of the caller cannot be passed on that way.
    void checkArrayArgument(const Procedure &callee, std::size_t i, const Variable &param,
                            Expr &arg)
    {
        const std::string which = "argument " + std::to_string(i + 1) + " of '" + callee.name + "'";
        const Variable *v = wholeArray(arg, which.c_str());
        if (v == nullptr || param.type == Type::Error)
        {
            return;
        }
        if (v->isDictionary != param.isDictionary)
        {
            diagnostics_.error(arg.location,
                               which + " must be " +
                                   (param.isDictionary ? "a Dictionary" : "an array") + ", not " +
                                   (v->isDictionary ? "a Dictionary" : "an array"));
            return;
        }
        if (v->isDictionary &&
            (v->type != param.type || v->enumDef != param.enumDef || v->typeDef != param.typeDef))
        {
            diagnostics_.error(arg.location, which + " must be a Dictionary(Of " +
                                                 name(infoOf(param)) + "), not (Of " +
                                                 name(infoOf(*v)) + ")");
            return;
        }
        if (v->type != param.type || v->enumDef != param.enumDef || v->typeDef != param.typeDef ||
            v->rank != param.rank)
        {
            diagnostics_.error(arg.location, which + " must be an array of " + name(infoOf(param)) +
                                                 " with " + dimensions(param.rank) + ", not of " +
                                                 name(infoOf(*v)) + " with " + dimensions(v->rank));
            return;
        }
        if (param.byRef)
        {
            readOnlyPath(arg, arg.location);
        }
    }

    // -- Errors ------------------------------------------------------------

    void checkTry(Stmt &s)
    {
        checkBlock(s.body);
        for (CatchClause &c : s.catches)
        {
            if (c.when != nullptr)
            {
                requireBoolean(*c.when, "a Catch When condition");
            }
            ++catchDepth_;
            checkBlock(c.body);
            --catchDepth_;
        }
        if (s.hasFinally)
        {
            finallyFloors_.push_back(loops_.size());
            checkBlock(s.finallyBody);
            finallyFloors_.pop_back();
        }
    }

    // Throw number [, description]; Err.Raise number [, source [, description]].
    void checkThrow(Stmt &s)
    {
        for (auto &arg : s.args)
        {
            checkExpr(*arg);
        }
        if (s.args.empty())
        {
            if (s.viaErrRaise)
            {
                diagnostics_.error(s.location, "Err.Raise needs an error number");
            }
            else if (catchDepth_ == 0)
            {
                diagnostics_.error(s.location, "a Throw on its own rethrows the error being "
                                               "handled, so it belongs inside a Catch");
            }
            return;
        }
        static const char *const kThrow[] = {"the error number", "the description"};
        static const char *const kRaise[] = {"the error number", "the source", "the description"};
        const char *const *names = s.viaErrRaise ? kRaise : kThrow;
        const std::size_t most = s.viaErrRaise ? 3 : 2;
        if (s.args.size() > most)
        {
            diagnostics_.error(s.location, s.viaErrRaise
                                               ? "Err.Raise takes a number, a source and a "
                                                 "description"
                                               : "Throw takes a number and a description");
            return;
        }
        for (std::size_t i = 0; i < s.args.size(); ++i)
        {
            const Type want = i == 0 ? Type::Integer : Type::String;
            const Type t = s.args[i]->type;
            if (!assignable(want, t))
            {
                diagnostics_.error(s.args[i]->location, std::string(names[i]) + " must be " +
                                                            article(want) + ", not " + article(t) +
                                                            conversionHint(want, t));
            }
        }
    }

    // Err.Number, Err.Description, Err.Source, Err.Line.
    Type resolveErrMember(Expr &e)
    {
        e.left->isErr = true;
        e.left->type = Type::Control;
        static const std::pair<const char *, Type> kMembers[] = {{"Number", Type::Integer},
                                                                 {"Description", Type::String},
                                                                 {"Source", Type::String},
                                                                 {"Line", Type::Integer}};
        for (const auto &[name, type] : kMembers)
        {
            if (e.text == name)
            {
                return type;
            }
        }
        diagnostics_.error(e.location, "Err has no property '" + e.text +
                                           "' (it has Number, Description, Source and Line)");
        return Type::Error;
    }

    // -- Loops and leaving them --------------------------------------------

    void checkFor(Stmt &s)
    {
        if (s.isForEach)
        {
            checkForEach(s);
            return;
        }
        const Type varType = checkExpr(*s.target);
        const TypeInfo var = infoOf(*s.target);
        const bool loopable =
            s.target->variable != nullptr && (isNumeric(varType) || varType == Type::Enum);
        if (varType != Type::Error && !loopable)
        {
            diagnostics_.error(s.target->location, "the For loop variable must be a numeric "
                                                   "variable (Integer, Long or Double) or an "
                                                   "Enum variable");
        }
        // Start and end are the variable's type; over an Enum the Step is a
        // plain number (For c = Color.Red To Color.Blue Step 2).
        auto requireFits = [&](Expr &e, const char *what, const TypeInfo &want) {
            checkExpr(e);
            const TypeInfo got = infoOf(e);
            if (loopable && !assignable(want, got))
            {
                // Converting a fractional Step would round it (0.5 to 0, a
                // loop that never ends): the variable's type is what to fix.
                const std::string hint =
                    isNumeric(got.type) && isNumeric(want.type)
                        ? std::string("; declare the loop variable As ") + typeName(got.type)
                        : conversionHint(want, got);
                diagnostics_.error(e.location, std::string("the ") + what + " of a For loop over " +
                                                   article(var) + " must be " + article(want) +
                                                   ", not " + article(got) + hint);
            }
        };
        requireFits(*s.value, "start", var);
        requireFits(*s.limit, "end", var);
        if (s.step != nullptr)
        {
            // Over an Enum or a Byte the Step is a plain number (it may be negative).
            requireFits(*s.step, "Step",
                        varType == Type::Enum   ? TypeInfo(lowered(var))
                        : varType == Type::Byte ? TypeInfo(Type::Integer)
                                                : var);
        }
        if (!s.nextName.empty() && s.nextName != s.target->text)
        {
            diagnostics_.error(s.nextLocation, "'Next " + s.nextName + "' does not match 'For " +
                                                   s.target->text + "'");
        }
        loops_.push_back(&s);
        checkBlock(s.body);
        loops_.pop_back();
    }

    void checkExit(Stmt &s)
    {
        if (s.exitWhat == "Sub" || s.exitWhat == "Function")
        {
            if (s.exitWhat != procedureKind())
            {
                diagnostics_.error(s.location, "'Exit " + s.exitWhat + "' cannot be used in a " +
                                                   procedureKind() + "; use 'Exit " +
                                                   procedureKind() + "'");
            }
            else if (!finallyFloors_.empty())
            {
                leavingFinally(s.location, "Exit " + s.exitWhat);
            }
            return;
        }
        for (std::size_t i = loops_.size(); i-- > 0;)
        {
            const Stmt &l = *loops_[i];
            const bool matches = s.exitWhat == "For"  ? l.kind == StmtKind::For
                                 : s.exitWhat == "Do" ? l.kind == StmtKind::Do && !l.isWhile
                                                      : l.kind == StmtKind::Do && l.isWhile;
            if (matches)
            {
                // A loop entirely inside the Finally may be left; one
                // around it may not.
                if (!finallyFloors_.empty() && i < finallyFloors_.back())
                {
                    leavingFinally(s.location, "Exit " + s.exitWhat);
                }
                s.exitLoop = &l;
                return;
            }
        }
        diagnostics_.error(s.location,
                           "'Exit " + s.exitWhat + "' is not inside a " + s.exitWhat + " loop");
    }

    // As in VB.NET: Finally runs while leaving, so it cannot itself leave.
    void leavingFinally(Location where, const std::string &statement)
    {
        diagnostics_.error(where, "'" + statement + "' cannot leave a Finally block");
    }

    void checkReturn(Stmt &s)
    {
        if (!finallyFloors_.empty())
        {
            leavingFinally(s.location, "Return");
        }
        if (!current_->isFunction)
        {
            if (s.value != nullptr)
            {
                checkExpr(*s.value);
                diagnostics_.error(s.value->location, "a Sub cannot return a value; make it a "
                                                      "Function");
            }
            return;
        }
        if (s.value == nullptr)
        {
            diagnostics_.error(s.location, "'Return' in a Function needs a value");
            return;
        }
        checkExpr(*s.value);
        requireAssignable(infoOf(*current_->result), infoOf(*s.value), s.value->location);
    }

    // -- Expressions -------------------------------------------------------

    Type checkExpr(Expr &e)
    {
        wholeNow_ = std::exchange(wantWhole_, false); // see checkWhole
        e.type = computeType(e);
        return e.type;
    }

    Type computeType(Expr &e)
    {
        switch (e.kind)
        {
            case ExprKind::IntegerLiteral:
                // Integer if it fits (or says %), else Long, as in VB.NET.
                if (e.suffix == '&')
                {
                    return Type::Long;
                }
                if (fitsInteger(e.integer))
                {
                    return Type::Integer;
                }
                if (e.suffix == '%')
                {
                    diagnostics_.error(e.location, "number does not fit in an Integer");
                    return Type::Error;
                }
                return Type::Long;
            case ExprKind::DoubleLiteral:
                return Type::Double;
            case ExprKind::Constant:
                return e.type; // set when the Const or member was replaced
            case ExprKind::New:
                if (auto it = types_.find(e.text); it != types_.end())
                {
                    e.typeDef = it->second;
                    return Type::Ref;
                }
                diagnostics_.error(e.location, "New makes a record of a Type, and '" + e.text +
                                                   "' is not a Type");
                return Type::Error;
            case ExprKind::Nothing:
                e.typeDef = nullptr;
                return Type::Ref;
            case ExprKind::Index:
                // a(i) was a Call the checks turned into an Index; o.Lines(i)
                // was parsed as one and is checked here.
                return e.left != nullptr && e.field == nullptr ? checkMemberIndex(e) : e.type;
            case ExprKind::StringLiteral:
                return Type::String;
            case ExprKind::BooleanLiteral:
                return Type::Boolean;
            case ExprKind::Name:
                return resolveName(e);
            case ExprKind::Member:
                return resolveMember(e);
            case ExprKind::Call:
                return checkCallExpr(e);
            case ExprKind::Unary:
                return checkUnary(e);
            case ExprKind::Binary:
                return checkBinary(e);
        }
        return Type::Error;
    }

    // Lookup order: locals and parameters, the current Function's result,
    // module variables, Functions, controls.
    Type resolveName(Expr &e)
    {
        Variable *v = current_ != nullptr ? lookupLocal(e.text) : nullptr;
        if (v == nullptr && current_ != nullptr && current_->isFunction && e.text == current_->name)
        {
            v = current_->result.get();
        }
        if (v == nullptr)
        {
            if (auto g = globals_.find(e.text); g != globals_.end())
            {
                v = g->second;
            }
        }

        if (v != nullptr && v->isConst)
        {
            if (!v->constReady)
            {
                if (v->type != Type::Error)
                {
                    diagnostics_.error(e.location,
                                       "Const '" + e.text + "' is used before its value is set");
                }
                return Type::Error;
            }
            return makeConstant(e, *v);
        }
        if (v != nullptr && v->isDictionary)
        {
            diagnostics_.error(e.location,
                               "'" + e.text + "' is a Dictionary; use an entry, as in " + e.text +
                                   "(\"key\"), or a member such as " + e.text + ".Count");
            return Type::Error;
        }
        if (v != nullptr && v->rank > 0)
        {
            const std::string commas(static_cast<std::size_t>(v->rank - 1), ',');
            diagnostics_.error(e.location,
                               "'" + e.text + "' is an array; use an element, as in " + e.text +
                                   "(" + (v->rank == 1 ? "0" : "0" + std::string(commas)) + ")");
            return Type::Error;
        }
        if (v != nullptr)
        {
            e.variable = v;
            e.enumDef = v->enumDef;
            e.typeDef = v->typeDef;
            if (e.suffix != 0 && suffixType(e.suffix) != v->type)
            {
                diagnostics_.error(e.location, "'" + e.text + e.suffix + "' does not match '" +
                                                   e.text + "', which is " + article(v->type));
                return Type::Error;
            }
            return v->type;
        }
        if (Procedure *proc = findProcedure(e.text))
        {
            // A bare Function name is a call with no arguments, as in VB.
            if (!proc->isFunction || !proc->parameters.empty())
            {
                return checkCallExpr(e); // reports what is wrong with the call
            }
            e.kind = ExprKind::Call;
            e.procedure = proc;
            return proc->result->type;
        }
        if (auto c = controls_.find(e.text); c != controls_.end())
        {
            e.control = c->second;
            return Type::Control;
        }

        if (e.text == "Err")
        {
            diagnostics_.error(e.location, "Err is used through its properties, as in Err.Number");
            return Type::Error;
        }
        if (types_.count(e.text) != 0)
        {
            diagnostics_.error(e.location, "'" + e.text +
                                               "' is a Type; declare a variable of it, "
                                               "as in Dim x As " +
                                               e.text);
            return Type::Error;
        }
        if (auto it = enums_.find(e.text); it != enums_.end())
        {
            const EnumDef &def = *it->second;
            diagnostics_.error(e.location, "'" + e.text + "' is an Enum; use one of its members" +
                                               (def.members.empty() ? std::string()
                                                                    : ", as in " + e.text + "." +
                                                                          def.members[0].name));
            return Type::Error;
        }
        if (const BuiltinInfo *b = findBuiltin(e.text, e.suffix))
        {
            // FreeFile: a function of no arguments is called by its name.
            if (const StringFunction *f = findStringFunction(b->id);
                f != nullptr && f->required == 0)
            {
                e.kind = ExprKind::Call;
                return checkStringFunction(e, b->name, *f);
            }
            diagnostics_.error(e.location, "'" + e.text +
                                               "' is a built-in function and needs an "
                                               "argument: " +
                                               e.text + "(...)");
            return Type::Error;
        }

        // A bare Enum member: the usual slip for VB6 programmers.
        for (const auto &[enumName, def] : enums_)
        {
            for (const EnumMember &m : def->members)
            {
                if (m.name == e.text)
                {
                    diagnostics_.error(e.location, "'" + e.text +
                                                       "' is not declared; Enum members "
                                                       "are written with their Enum: " +
                                                       enumName + "." + m.name);
                    return Type::Error;
                }
            }
        }

        std::string message = "'" + e.text + "' is not declared (add a Dim)";
        for (const auto &scope : scopes_)
        {
            for (const auto &[name, var] : scope)
            {
                if (catalog::equalsIgnoringCase(name, e.text))
                {
                    message = "'" + e.text + "' is not declared; did you mean '" + name +
                              "'? (names are case-sensitive)";
                }
            }
        }
        for (const auto &[name, var] : globals_)
        {
            if (catalog::equalsIgnoringCase(name, e.text))
            {
                message = "'" + e.text + "' is not declared; did you mean '" + name +
                          "'? (names are case-sensitive)";
            }
        }
        for (const auto &[name, def] : controls_)
        {
            if (catalog::equalsIgnoringCase(name, e.text))
            {
                message = "'" + e.text + "' is not declared; did you mean the control '" + name +
                          "'? (names are case-sensitive)";
            }
        }
        diagnostics_.error(e.location, message);
        return Type::Error;
    }

    Type resolveMember(Expr &e)
    {
        const bool whole = wholeNow_;
        if (e.left == nullptr) // .Name: a member of the innermost With's target
        {
            if (withs_.empty() || withs_.back() == nullptr)
            {
                if (withs_.empty())
                {
                    diagnostics_.error(e.location,
                                       "'." + e.text + "' needs a With block around it");
                }
                e.left = std::make_unique<Expr>(); // an error, already reported
                e.left->kind = ExprKind::Name;
                e.left->location = e.location;
                return Type::Error;
            }
            e.left = clonePath(*withs_.back(), e.location);
        }
        Expr &object = *e.left;
        if (object.kind == ExprKind::Name && object.text == "Err")
        {
            return resolveErrMember(e);
        }
        if (object.kind == ExprKind::Name)
        {
            if (auto it = enums_.find(object.text); it != enums_.end())
            {
                return resolveEnumMember(e, *it->second);
            }
        }
        if (const Variable *d = dictionaryOf(object))
        {
            return dictionaryMember(e, *d, whole);
        }
        const Type objectType = checkExpr(object);
        if (objectType == Type::Error)
        {
            return Type::Error;
        }
        if (objectType == Type::Record && object.rank == 0)
        {
            return wholeWanted(e, resolveField(e, *object.typeDef), whole);
        }
        if (objectType == Type::Ref && object.rank == 0)
        {
            if (object.typeDef == nullptr)
            {
                diagnostics_.error(object.location, "Nothing has no members");
                return Type::Error;
            }
            return wholeWanted(e, resolveField(e, *object.typeDef),
                               whole); // the record it refers to
        }
        if (object.control == nullptr)
        {
            diagnostics_.error(object.location, describeObject(object) + " is " +
                                                    article(infoOf(object)) +
                                                    " and has no members or properties");
            return Type::Error;
        }
        const catalog::ControlType *type = catalog::findType(object.control->typeName);
        if (type == nullptr)
        {
            return Type::Error;
        }
        const catalog::Property *prop = catalog::findProperty(*type, e.text);
        if (prop == nullptr)
        {
            unknownProperty(*type, e.text, e.location);
            return Type::Error;
        }
        return prop->type;
    }

    Type checkUnary(Expr &e)
    {
        const Type t = checkExpr(*e.left);
        if (t == Type::Error)
        {
            return Type::Error;
        }
        if (e.text == "-")
        {
            const Type n = lowered(infoOf(*e.left)); // -Color.Red is a number
            if (!isNumeric(n))
            {
                diagnostics_.error(e.location, std::string("cannot negate ") + article(t));
                return Type::Error;
            }
            return arithmeticType(n, n); // -b is an Integer
        }
        // Not
        if (t != Type::Boolean)
        {
            diagnostics_.error(e.location, std::string("'Not' needs a Boolean, not ") + article(t));
            return Type::Error;
        }
        return Type::Boolean;
    }

    // Arithmetic on mixed numeric types is done in the wider type; / and ^
    // always give a Double, as in VB.
    Type checkBinary(Expr &e)
    {
        Type l = checkExpr(*e.left);
        Type r = checkExpr(*e.right);
        if (l == Type::Error || r == Type::Error)
        {
            return Type::Error;
        }
        const std::string &op = e.text;
        // An Enum takes part as its number; two different Enums are never
        // compared, which is what keeping them distinct is for.
        const bool comparison =
            op == "=" || op == "<>" || op == "<" || op == ">" || op == "<=" || op == ">=";
        if (comparison && l == Type::Enum && r == Type::Enum && e.left->enumDef != e.right->enumDef)
        {
            return operandError(e, l, r);
        }
        if (op == "Is" || op == "IsNot")
        {
            const bool refs = l == Type::Ref && r == Type::Ref &&
                              (e.left->typeDef == e.right->typeDef || e.left->typeDef == nullptr ||
                               e.right->typeDef == nullptr);
            if (!refs)
            {
                diagnostics_.error(e.location, "'" + op +
                                                   "' compares two Refs of the same Type (or "
                                                   "Nothing), not " +
                                                   article(infoOf(*e.left)) + " and " +
                                                   article(infoOf(*e.right)));
                return Type::Error;
            }
            return Type::Boolean;
        }
        if (comparison && (l == Type::Ref || r == Type::Ref))
        {
            diagnostics_.error(e.location, "compare Refs with 'Is' or 'IsNot', not '" + op + "'");
            return Type::Error;
        }
        if (comparison && (l == Type::Record || r == Type::Record))
        {
            diagnostics_.error(e.location,
                               "Types cannot be compared with '" + op + "'; compare their members");
            return Type::Error;
        }
        l = lowered(infoOf(*e.left));
        r = lowered(infoOf(*e.right));
        const bool numeric = isNumeric(l) && isNumeric(r);

        if (op == "&")
        {
            // The one implicit conversion: numbers and Booleans become text.
            if (l == Type::Control || r == Type::Control || l == Type::Record ||
                r == Type::Record || l == Type::Ref || r == Type::Ref)
            {
                return operandError(e, l, r);
            }
            return Type::String;
        }
        if (op == "+" && l == Type::String && r == Type::String)
        {
            return Type::String;
        }
        if (op == "+" || op == "-" || op == "*" || op == "Mod")
        {
            return numeric ? arithmeticType(l, r) : operandError(e, l, r);
        }
        if (op == "/" || op == "^")
        {
            return numeric ? Type::Double : operandError(e, l, r);
        }
        if (op == "\\")
        {
            if (numeric && (l == Type::Double || r == Type::Double))
            {
                diagnostics_.error(e.location, "'\\' divides Integers and Longs; use '/' for a "
                                               "Double, or convert with CLng first");
                return Type::Error;
            }
            return numeric ? arithmeticType(l, r) : operandError(e, l, r);
        }
        if (op == "And" || op == "Or")
        {
            if (l == Type::Boolean && r == Type::Boolean)
            {
                return Type::Boolean;
            }
            return operandError(e, l, r);
        }
        // Comparisons: two numbers of any types, or the same comparable type.
        if (numeric ||
            (l == r && (l == Type::String || (l == Type::Boolean && (op == "=" || op == "<>")))))
        {
            return Type::Boolean;
        }
        return operandError(e, l, r);
    }

    Type operandError(const Expr &e, Type, Type)
    {
        diagnostics_.error(e.location, "operator '" + e.text + "' cannot combine " +
                                           article(infoOf(*e.left)) + " and " +
                                           article(infoOf(*e.right)));
        return Type::Error;
    }

    // -- Whole program -----------------------------------------------------

    void checkEntryPoint()
    {
        if (const Procedure *main = findProcedure("Main"))
        {
            if (main->isFunction || !main->parameters.empty())
            {
                diagnostics_.error(main->location, "Main must be a Sub with no parameters");
            }
        }
        else if (module_.form == nullptr)
        {
            diagnostics_.error({1, 1}, "nothing to run: add a form (Begin Form ... End) or a "
                                       "Sub Main");
        }
    }

    // -- Helpers -----------------------------------------------------------

    static bool fitsInteger(std::int64_t value) { return value >= INT32_MIN && value <= INT32_MAX; }

    static Type suffixType(char suffix)
    {
        switch (suffix)
        {
            case '%':
                return Type::Integer;
            case '&':
                return Type::Long;
            case '#':
                return Type::Double;
            case '$':
                return Type::String;
            default:
                return Type::Error;
        }
    }

    // "an Integer", "a Color" (an Enum by its name).
    static std::string article(const TypeInfo &info)
    {
        if (info.type == Type::Ref)
        {
            return info.record != nullptr ? "a Ref " + info.record->name : "Nothing";
        }
        const std::string *named = info.type == Type::Enum && info.def != nullptr ? &info.def->name
                                   : info.type == Type::Record && info.record != nullptr
                                       ? &info.record->name
                                       : nullptr;
        if (named != nullptr)
        {
            const char c = named->empty() ? 'x' : (*named)[0];
            const bool vowel = std::string("AEIOUaeiou").find(c) != std::string::npos;
            return (vowel ? "an " : "a ") + *named;
        }
        switch (info.type)
        {
            case Type::Byte:
                return "a Byte";
            case Type::Integer:
                return "an Integer";
            case Type::Long:
                return "a Long";
            case Type::Double:
                return "a Double";
            case Type::String:
                return "a String";
            case Type::Boolean:
                return "a Boolean";
            case Type::Control:
                return "a control";
            default:
                return "an unknown value";
        }
    }

    Module &module_;
    Diagnostics &diagnostics_;
    std::map<std::string, std::string> moduleNames_; // name -> what it is
    std::map<std::string, ControlDef *> controls_;   // includes the form
    std::map<std::string, Variable *> globals_;
    std::map<std::string, Procedure *> procedures_;

    // The procedure being checked: its scopes, innermost last, and the
    // loops around the current statement, innermost last.
    Procedure *current_ = nullptr;
    std::vector<std::map<std::string, Variable *>> scopes_;
    std::vector<const Stmt *> loops_;
    std::map<std::string, EnumDef *> enums_;
    std::map<std::string, TypeDef *> types_;
    bool foldReported_ = false; // fold() has reported why a constant failed
    int catchDepth_ = 0;        // Catch blocks around the statement (for a bare Throw)
    // For each Finally around the statement, how many loops were already
    // open when it began: those cannot be left from inside it.
    std::vector<std::size_t> finallyFloors_;
    // The paths of the Withs around the statement, innermost last (null for
    // one whose target was in error); hidden locals made so far.
    std::vector<const Expr *> withs_;
    int withCount_ = 0;
    bool wantWhole_ = false; // see checkWhole
    bool wholeNow_ = false;  // the expression being checked may be a whole array
};

} // namespace

bool checkModule(ast::Module &module, Diagnostics &diagnostics)
{
    return Checker(module, diagnostics).run();
}

} // namespace tf
