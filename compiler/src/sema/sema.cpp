// SPDX-License-Identifier: BSD-2-Clause
//
// sema.cpp -- see sema.hpp.

#include "sema/sema.hpp"

#include "sema/catalog.hpp"

#include <cstdint>
#include <map>
#include <set>

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

constexpr BuiltinInfo kBuiltins[] = {{"CInt", Builtin::CInt, false}, {"CLng", Builtin::CLng, false},
                                     {"CDbl", Builtin::CDbl, false}, {"CStr", Builtin::CStr, false},
                                     {"Str", Builtin::Str, true},    {"Val", Builtin::Val, false},
                                     {"Fix", Builtin::Fix, false},   {"Int", Builtin::Int, false}};

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
        registerControls();
        registerGlobals();
        registerProcedures();
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
            if (v->type != Type::Error && claimModuleName(v->name, v->location, "variable"))
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
            case StmtKind::End:
                break;
        }
    }

    const char *procedureKind() const { return current_->isFunction ? "Function" : "Sub"; }

    void declareLocal(Variable &v)
    {
        if (v.type == Type::Error)
        {
            return; // already reported by the parser
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
        const Type valueType = checkExpr(*s.value);
        const Type targetType = checkExpr(target);

        if (target.kind == ExprKind::Name && target.control != nullptr)
        {
            diagnostics_.error(target.location, "cannot assign to the control '" + target.text +
                                                    "' itself; assign to one of its properties");
            return;
        }
        if (target.kind == ExprKind::Name || target.kind == ExprKind::Member)
        {
            requireAssignable(targetType, valueType, s.value->location);
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

    // Option Strict: the same type, or a numeric widening.
    static bool assignable(Type target, Type value)
    {
        if (target == Type::Error || value == Type::Error || target == value)
        {
            return true;
        }
        return isNumeric(target) && isNumeric(value) && widerNumeric(target, value) == target;
    }

    void requireAssignable(Type target, Type value, Location where)
    {
        if (!assignable(target, value))
        {
            diagnostics_.error(where, std::string("cannot assign ") + article(value) + " to " +
                                          article(target) + " (Option Strict)" +
                                          conversionHint(target, value));
        }
    }

    // "; use CInt(...) to convert", or nothing when no conversion exists.
    static std::string conversionHint(Type target, Type value)
    {
        const char *convert = target == Type::Integer  ? "CInt"
                              : target == Type::Long   ? "CLng"
                              : target == Type::Double ? "CDbl"
                              : target == Type::String ? "CStr"
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
        for (auto &arg : args)
        {
            checkExpr(*arg);
        }
        const std::size_t want = callee.parameters.size();
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
            if (param.type == Type::Error || arg.type == Type::Error)
            {
                continue;
            }
            const std::string which =
                "argument " + std::to_string(i + 1) + " of '" + callee.name + "'";
            if (param.byRef)
            {
                if (arg.kind != ExprKind::Name || arg.variable == nullptr)
                {
                    diagnostics_.error(arg.location, which + " is ByRef, so it must be a variable");
                }
                else if (arg.type != param.type)
                {
                    diagnostics_.error(arg.location, which + " is ByRef " + typeName(param.type) +
                                                         ", so it must be " + article(param.type) +
                                                         " variable, not " + article(arg.type));
                }
                continue;
            }
            if (!assignable(param.type, arg.type))
            {
                diagnostics_.error(arg.location, which + " must be " + article(param.type) +
                                                     ", not " + article(arg.type) +
                                                     " (Option Strict)" +
                                                     conversionHint(param.type, arg.type));
            }
        }
    }

    Type checkCallExpr(Expr &e)
    {
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
        return e.procedure->result->type;
    }

    // Conversions and number functions; each takes one argument.
    Type checkBuiltin(Expr &e, const BuiltinInfo &b)
    {
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
        const Type t = e.args[0]->type;
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
            case Builtin::CLng:
            case Builtin::CDbl:
                if (!isNumeric(t) && t != Type::String)
                {
                    return wrongArgument("a number or a String");
                }
                return b.id == Builtin::CInt   ? Type::Integer
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
            case Builtin::None:
                break;
        }
        return Type::Error;
    }

    // -- Loops and leaving them --------------------------------------------

    void checkFor(Stmt &s)
    {
        const Type varType = checkExpr(*s.target);
        const bool numericVariable = s.target->variable != nullptr && isNumeric(varType);
        if (varType != Type::Error && !numericVariable)
        {
            diagnostics_.error(s.target->location, "the For loop variable must be a numeric "
                                                   "variable (Integer, Long or Double)");
        }
        auto requireFits = [&](Expr &e, const char *what) {
            const Type t = checkExpr(e);
            if (numericVariable && !assignable(varType, t))
            {
                // Converting a fractional Step would round it (0.5 to 0, a
                // loop that never ends): the variable's type is what to fix.
                const std::string hint =
                    isNumeric(t) ? std::string("; declare the loop variable As ") + typeName(t)
                                 : conversionHint(varType, t);
                diagnostics_.error(e.location, std::string("the ") + what + " of a For loop over " +
                                                   article(varType) + " must be " +
                                                   article(varType) + ", not " + article(t) + hint);
            }
        };
        requireFits(*s.value, "start");
        requireFits(*s.limit, "end");
        if (s.step != nullptr)
        {
            requireFits(*s.step, "Step");
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
            return;
        }
        for (auto loop = loops_.rbegin(); loop != loops_.rend(); ++loop)
        {
            const Stmt &l = **loop;
            const bool matches = s.exitWhat == "For"  ? l.kind == StmtKind::For
                                 : s.exitWhat == "Do" ? l.kind == StmtKind::Do && !l.isWhile
                                                      : l.kind == StmtKind::Do && l.isWhile;
            if (matches)
            {
                s.exitLoop = &l;
                return;
            }
        }
        diagnostics_.error(s.location,
                           "'Exit " + s.exitWhat + "' is not inside a " + s.exitWhat + " loop");
    }

    void checkReturn(Stmt &s)
    {
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
        requireAssignable(current_->result->type, checkExpr(*s.value), s.value->location);
    }

    // -- Expressions -------------------------------------------------------

    Type checkExpr(Expr &e)
    {
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

        if (v != nullptr)
        {
            e.variable = v;
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

        if (findBuiltin(e.text, e.suffix) != nullptr)
        {
            diagnostics_.error(e.location, "'" + e.text +
                                               "' is a built-in function and needs an "
                                               "argument: " +
                                               e.text + "(...)");
            return Type::Error;
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
        Expr &object = *e.left;
        const Type objectType = checkExpr(object);
        if (objectType == Type::Error)
        {
            return Type::Error;
        }
        if (object.control == nullptr)
        {
            diagnostics_.error(object.location, "'" + object.text + "' is " + article(objectType) +
                                                    " and has no properties");
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
            if (!isNumeric(t))
            {
                diagnostics_.error(e.location, std::string("cannot negate ") + article(t));
                return Type::Error;
            }
            return t;
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
        const Type l = checkExpr(*e.left);
        const Type r = checkExpr(*e.right);
        if (l == Type::Error || r == Type::Error)
        {
            return Type::Error;
        }
        const std::string &op = e.text;
        const bool numeric = isNumeric(l) && isNumeric(r);

        if (op == "&")
        {
            // The one implicit conversion: numbers and Booleans become text.
            if (l == Type::Control || r == Type::Control)
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
            return numeric ? widerNumeric(l, r) : operandError(e, l, r);
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
            return numeric ? widerNumeric(l, r) : operandError(e, l, r);
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

    Type operandError(const Expr &e, Type l, Type r)
    {
        diagnostics_.error(e.location, "operator '" + e.text + "' cannot combine " + article(l) +
                                           " and " + article(r));
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

    static std::string article(Type t)
    {
        switch (t)
        {
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
};

} // namespace

bool checkModule(ast::Module &module, Diagnostics &diagnostics)
{
    return Checker(module, diagnostics).run();
}

} // namespace tf
