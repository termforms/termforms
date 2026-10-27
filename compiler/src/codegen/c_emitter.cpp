// SPDX-License-Identifier: BSD-2-Clause
//
// c_emitter.cpp -- see c_emitter.hpp.
//
// String ownership in generated code (see tf_runtime.h for the runtime side):
// an expression either borrows a string (a variable, a literal, a property
// read) or owns one (+1, the result of &, a conversion or a Function). An
// owned value that is only read is parked in a temporary and released at the
// end of the statement; an owned value being stored is handed over without a
// copy.
//
// Evaluation order is left to right, as in VB.NET, whatever C compiler
// builds the program (C leaves the order of operands unspecified). Each
// Function call in an expression is made ahead of the statement, in order,
// into its own temporary. A value read before such a call, which the call
// could change (a module variable, a ByRef parameter, a local passed ByRef
// in the same statement, a control property), is copied into a temporary
// first. That copy also keeps a String alive if the call reassigns the
// variable it came from.
//
// Errors (Try/Catch/Finally): the runtime sets tf_error_pending and
// returns; generated code tests it after anything that can fail and jumps
// to the innermost handler: the Try's Catch dispatch (fail_<n>), its
// Finally (finally_<n>, from inside a Catch), a cleanup that drops the
// pending error a Finally set aside (ffail_<n>, from inside the Finally),
// or the procedure's cleanup (done), after which the caller tests it in
// turn. A value that can fail is computed into a temporary and checked
// before it is stored or passed on, so a failed statement has no partial
// effect. Leaving a Try with Exit or Return records the way out in
// leave_<n> and runs the Finally first. A program with no Try gets no
// checks at all: nothing could catch an error, so the runtime ends the
// program where the error happens, as it always has.
//
// Procedures: parameters are C parameters (ByRef ones are pointers). A
// ByVal String parameter takes its own reference on entry, so the callee
// may assign to it, and the caller may change the variable it came from,
// without either freeing the other's string. Locals in nested blocks share
// storage that lasts the whole call; a Dim inside a loop resets its
// variables each time it runs. Leaving early (Exit Sub, Return) jumps to the
// cleanup at End Sub when there are strings to release.

#include "codegen/c_emitter.hpp"

#include "sema/catalog.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace tf
{

namespace
{

using namespace tf::ast;

// A C expression for a value, and whether it holds a string reference that
// someone must release.
struct Value
{
    std::string code;
    bool owned = false;
};

// A Try being emitted: where errors inside it go, and the ways out of it
// (Exit, Return) that must run its Finally first.
struct TryContext
{
    enum class Part
    {
        Body,
        Catch,
        Finally
    };
    int id = 0;
    Part part = Part::Body;
    bool hasCatches = false;
    std::size_t loopFloor = 0;        // loops already open at Try
    std::vector<const Stmt *> leaves; // per leave_<n> value: the loop, or null for the procedure
    bool failUsed = false;            // fail_<n> is jumped to
    bool finallyFailUsed = false;     // ffail_<n> is jumped to
};

// Per-statement scratch: temporaries and the code that fills them.
struct StatementScope
{
    std::vector<std::string> prelude;  // "tf_string *t1 = ...;", run first
    std::vector<std::string> releases; // "tf_str_release(t1);": run after the statement
    int temps = 0;                     // temporaries so far, for t1, t2, ...

    // Function calls in the statement not yet made, and locals the
    // statement passes ByRef to a Function (so a call may change them).
    int pendingCalls = 0;
    std::set<const Variable *> passedByRef;

    std::string newTemp() { return "t" + std::to_string(++temps); }
};

class Emitter
{
public:
    Emitter(const Module &module, const EmitOptions &options) : module_(module), options_(options)
    {
    }

    std::string run()
    {
        checks_ = containsTry(module_);
        std::ostringstream body;
        for (const auto &proc : module_.procedures)
        {
            emitProcedure(body, *proc);
        }
        emitMain(body);

        std::ostringstream out;
        emitHeader(out);
        out << body.str();
        return out.str();
    }

private:
    // -- Types (records) -----------------------------------------------------

    // Each Type becomes a C struct (members f_<name>) and four helpers the
    // runtime and generated code share through its tf_record_ops: init (a
    // zeroed record, its fixed arrays sized), free, copy (deep: strings
    // retained, arrays and nested records copied) and, if it holds arrays,
    // busy (an array in it is locked, so it must not be overwritten).
    void emitTypes(std::ostringstream &out)
    {
        if (module_.types.empty())
        {
            return;
        }
        out << "/* Types */\n";
        for (const auto &def : module_.types)
        {
            out << "typedef struct t_" << def->name << " t_" << def->name << ";\n";
        }
        out << '\n';
        std::set<const TypeDef *> done;
        for (const auto &def : module_.types)
        {
            emitStruct(out, *def, done);
        }
        // The helpers are written first: their array members may need ops too.
        std::ostringstream helpers;
        for (const auto &def : module_.types)
        {
            emitTypeHelpers(helpers, *def);
        }
        for (const auto &def : module_.types)
        {
            const std::string t = "t_" + def->name;
            out << "static void " << t << "_init(void *record);\n"
                << "static void " << t << "_free(void *record);\n"
                << "static void " << t << "_copy(void *to, const void *from);\n";
            if (holdsArrays(*def))
            {
                out << "static int " << t << "_busy(const " << t << " *x);\n";
            }
            if (opsNeeded_.count(def.get()) != 0) // only arrays and Refs of the Type need it
            {
                out << "static const tf_record_ops " << t << "_ops = {sizeof(" << t << "), " << t
                    << "_init, " << t << "_free, " << t << "_copy};\n";
            }
        }
        out << '\n' << helpers.str();
    }

    // A struct after the structs it holds by value.
    void emitStruct(std::ostringstream &out, const TypeDef &def, std::set<const TypeDef *> &done)
    {
        if (!done.insert(&def).second)
        {
            return;
        }
        for (const auto &m : def.members)
        {
            if (m->type == Type::Record && m->rank == 0)
            {
                emitStruct(out, *m->typeDef, done);
            }
        }
        out << "struct t_" << def.name << "\n{\n";
        for (const auto &m : def.members)
        {
            out << "    "
                << (m->rank > 0 ? "tf_array f_" + m->name : declarationOf(*m, "f_" + m->name))
                << ";\n";
        }
        out << "};\n\n";
    }

    // Does a record of this Type hold any array, directly or in a member?
    static bool holdsArrays(const TypeDef &def)
    {
        for (const auto &m : def.members)
        {
            if (m->rank > 0 || (m->type == Type::Record && holdsArrays(*m->typeDef)))
            {
                return true;
            }
        }
        return false;
    }

    void emitTypeHelpers(std::ostringstream &out, const TypeDef &def)
    {
        const std::string t = "t_" + def.name;

        out << "static void " << t << "_init(void *record)\n{\n"
            << "    " << t << " *x = record;\n"
            << "    const " << t << " empty = {0};\n"
            << "    *x = empty;\n";
        for (const auto &m : def.members)
        {
            if (m->rank > 0 && !m->isDynamic)
            {
                out << "    " << arrayInit(*m, "&x->f_" + m->name) << '\n';
            }
            else if (m->rank == 0 && m->type == Type::Record)
            {
                out << "    t_" << m->typeDef->name << "_init(&x->f_" << m->name << ");\n";
            }
        }
        out << "}\n\n";

        out << "static void " << t << "_free(void *record)\n{\n"
            << "    " << t << " *x = record;\n";
        bool freed = false;
        for (const auto &m : def.members)
        {
            const std::string f = "x->f_" + m->name;
            if (m->rank > 0)
            {
                out << "    tf_array_free(&" << f << ");\n";
            }
            else if (m->type == Type::String)
            {
                out << "    tf_str_release(" << f << ");\n";
            }
            else if (m->type == Type::Ref)
            {
                out << "    tf_ref_release(" << f << ");\n";
            }
            else if (m->type == Type::Record)
            {
                out << "    t_" << m->typeDef->name << "_free(&" << f << ");\n";
            }
            else
            {
                continue;
            }
            freed = true;
        }
        if (!freed)
        {
            out << "    (void)x;\n";
        }
        out << "}\n\n";

        // Built in a fresh record, then moved in, so copying a record into
        // one that holds it (c = c.Kids(0)) never reads freed memory.
        out << "static void " << t << "_copy(void *to, const void *from)\n{\n"
            << "    " << t << " *x = to;\n"
            << "    const " << t << " *y = from;\n"
            << "    " << t << " fresh;\n"
            << "    if (x == y)\n    {\n        return;\n    }\n";
        if (holdsArrays(def))
        {
            out << "    if (" << t << "_busy(x))\n    {\n"
                << "        tf_raise(TF_ERR_ARRAY_LOCKED, NULL);\n        return;\n    }\n";
        }
        out << "    " << t << "_init(&fresh);\n";
        for (const auto &m : def.members)
        {
            const std::string to = "fresh.f_" + m->name;
            const std::string from = "y->f_" + m->name;
            if (m->rank > 0)
            {
                out << "    tf_array_assign(&" << to << ", &" << from << ");\n";
            }
            else if (m->type == Type::String)
            {
                out << "    " << to << " = tf_str_ref(" << from << ");\n";
            }
            else if (m->type == Type::Ref)
            {
                out << "    " << to << " = tf_ref_ref(" << from << ");\n";
            }
            else if (m->type == Type::Record)
            {
                out << "    t_" << m->typeDef->name << "_copy(&" << to << ", &" << from << ");\n";
            }
            else
            {
                out << "    " << to << " = " << from << ";\n";
            }
        }
        out << "    " << t << "_free(x);\n"
            << "    *x = fresh;\n}\n\n";

        if (holdsArrays(def))
        {
            std::string busy;
            for (const auto &m : def.members)
            {
                if (m->rank > 0)
                {
                    busy += (busy.empty() ? "" : " || ") + std::string("x->f_") + m->name +
                            ".locks > 0";
                }
                else if (m->type == Type::Record && holdsArrays(*m->typeDef))
                {
                    busy += (busy.empty() ? "" : " || ") + std::string("t_") + m->typeDef->name +
                            "_busy(&x->f_" + m->name + ")";
                }
            }
            out << "static int " << t << "_busy(const " << t << " *x)\n{\n"
                << "    return " << busy << ";\n}\n\n";
        }
    }

    // -- File-level parts --------------------------------------------------

    void emitHeader(std::ostringstream &out)
    {
        out << "/* Generated by tfc " << options_.version << " from " << options_.sourcePath
            << ".\n"
            << " * Do not edit: changes are lost on the next build. */\n"
            << "#include \"termforms/tf_runtime.h\"\n\n";
        emitTypes(out);

        if (module_.form != nullptr)
        {
            out << "/* Form and controls */\n";
            out << "static tf_control *c_" << module_.form->name << ";\n";
            for (const auto &child : module_.form->children)
            {
                out << "static tf_control *c_" << child->name << ";\n";
            }
            out << '\n';
        }

        if (std::any_of(module_.globals.begin(), module_.globals.end(),
                        [](const auto &v) { return !v->isConst; }))
        {
            out << "/* Module-level variables */\n";
            for (const auto &v : module_.globals)
            {
                if (!v->isConst) // a Const's uses are its value: it needs no storage
                {
                    out << "static "
                        << (v->rank > 0 ? "tf_array g_" + v->name
                                        : declarationOf(*v, "g_" + v->name))
                        << ";\n";
                }
            }
            out << '\n';
        }

        if (!literals_.empty())
        {
            out << "/* String literals: created on first use, never freed */\n";
            for (std::size_t i = 0; i < literals_.size(); ++i)
            {
                out << "static tf_string *s_" << i << ";\n";
            }
            out << '\n';
        }

        if (!module_.procedures.empty())
        {
            out << "/* Subs and Functions */\n";
            for (const auto &proc : module_.procedures)
            {
                out << signature(*proc) << ";\n";
            }
            out << '\n';
        }
    }

    // "static int32_t p_Area(int32_t v_w, int32_t v_h)"
    static std::string signature(const Procedure &proc)
    {
        std::string out = "static ";
        out += proc.isFunction ? cTypeOf(*proc.result) : "void";
        if (out.back() != '*')
        {
            out += ' ';
        }
        out += "p_" + proc.name + "(";
        if (proc.parameters.empty())
        {
            out += "void";
        }
        for (std::size_t i = 0; i < proc.parameters.size(); ++i)
        {
            const Variable &p = *proc.parameters[i];
            // A ByVal record comes by pointer (a_<name>) and is copied into the
            // local v_<name> on entry.
            out += (i == 0 ? "" : ", ") +
                   (p.rank > 0 ? "tf_array *v_" + p.name
                    : p.type == Type::Record && !p.byRef
                        ? "const " + cTypeOf(p) + " *a_" + p.name
                        : declarationOf(p, std::string(p.byRef ? "*" : "") + "v_" + p.name));
        }
        return out + ")";
    }

    void emitProcedure(std::ostringstream &out, const Procedure &proc)
    {
        current_ = &proc;
        assignLocalNames(proc);
        loopIds_.clear();
        loopLabels_.clear();
        loopCount_ = 0;
        tries_.clear();
        tryCount_ = 0;

        line(out, proc.location);
        out << signature(proc) << "\n{\n";

        for (const auto &v : proc.locals)
        {
            if (v->type != Type::Error && !v->isConst)
            {
                out << "    "
                    << (v->rank > 0 ? "tf_array " + names_.at(v.get()) + " = {0}"
                        : v->type == Type::Record
                            ? declarationOf(*v, names_.at(v.get()))
                            : declarationOf(*v, names_.at(v.get())) + " = " + zero(v->type))
                    << ";\n";
            }
        }
        for (const auto &p : proc.parameters) // a ByVal record: the callee's own copy
        {
            if (p->type == Type::Record && !p->byRef && p->rank == 0)
            {
                out << "    t_" << p->typeDef->name << " v_" << p->name << ";\n";
            }
        }
        for (const auto &v : proc.locals) // fixed arrays get their size at once
        {
            if (v->rank > 0 && !v->isDynamic && v->type != Type::Error)
            {
                out << "    " << arrayInit(*v, "&" + names_.at(v.get())) << '\n';
            }
            else if (v->rank == 0 && v->type == Type::Record && !v->isConst)
            {
                out << "    t_" << v->typeDef->name << "_init(&" << names_.at(v.get()) << ");\n";
            }
        }
        for (const auto &p : proc.parameters)
        {
            if (p->type == Type::Record && !p->byRef && p->rank == 0)
            {
                const std::string t = "t_" + p->typeDef->name;
                out << "    " << t << "_init(&v_" << p->name << "); " << t << "_copy(&v_" << p->name
                    << ", a_" << p->name << ");\n";
            }
        }
        if (proc.isFunction && proc.result->type == Type::Record)
        {
            // Returned by value: the caller owns (and frees) what it gets.
            out << "    " << declarationOf(*proc.result, "r_" + proc.name) << ";\n"
                << "    t_" << proc.result->typeDef->name << "_init(&r_" << proc.name << ");\n";
        }
        else if (proc.isFunction)
        {
            out << "    " << declarationOf(*proc.result, "r_" + proc.name) << " = "
                << zero(proc.result->type) << ";\n";
        }
        for (const auto &p : proc.parameters)
        {
            if (p->type == Type::String && !p->byRef)
            {
                out << "    tf_str_retain(v_" << p->name << ");\n";
            }
            else if (p->type == Type::Ref && !p->byRef && p->rank == 0)
            {
                out << "    tf_ref_retain(v_" << p->name << ");\n";
            }
        }

        // The cleanup at End Sub: strings to release, then the result.
        cleanup_.clear();
        for (const auto &p : proc.parameters)
        {
            if (p->type == Type::String && !p->byRef)
            {
                cleanup_ += " tf_str_release(v_" + p->name + ");";
            }
            else if (p->type == Type::Record && !p->byRef && p->rank == 0)
            {
                cleanup_ += " t_" + p->typeDef->name + "_free(&v_" + p->name + ");";
            }
            else if (p->type == Type::Ref && !p->byRef && p->rank == 0)
            {
                cleanup_ += " tf_ref_release(v_" + p->name + ");";
            }
        }
        for (const auto &v : proc.locals)
        {
            if (v->rank > 0 && v->type != Type::Error)
            {
                cleanup_ += " tf_array_free(&" + names_.at(v.get()) + ");";
            }
            else if (v->type == Type::Record && !v->isConst)
            {
                cleanup_ += " t_" + v->typeDef->name + "_free(&" + names_.at(v.get()) + ");";
            }
            else if (v->type == Type::Ref)
            {
                cleanup_ += " tf_ref_release(" + names_.at(v.get()) + ");";
            }
            else if (v->type == Type::String && !v->isConst)
            {
                cleanup_ += " tf_str_release(" + names_.at(v.get()) + ");";
            }
        }
        usesCleanupLabel_ = false;

        emitBlock(out, proc.body, 1);

        const std::string result = proc.isFunction ? " return r_" + proc.name + ";" : "";
        if (!cleanup_.empty() || !result.empty() || usesCleanupLabel_)
        {
            line(out, proc.endLocation);
            const std::string rest = cleanup_ + result;
            out << (usesCleanupLabel_ ? "done:" : "   ") << (rest.empty() ? ";" : rest) << '\n';
        }
        out << "}\n\n";
        current_ = nullptr;
    }

    // v_<name> for each parameter and local; a block-scoped name used again
    // in a later block of the same procedure gets v_<name>_2, _3, ...
    void assignLocalNames(const Procedure &proc)
    {
        names_.clear();
        std::map<std::string, int> uses;
        for (const auto &p : proc.parameters)
        {
            names_[p.get()] = "v_" + p->name;
            uses[p->name] = 1;
        }
        for (const auto &v : proc.locals)
        {
            const int n = ++uses[v->name];
            names_[v.get()] = "v_" + v->name + (n == 1 ? "" : "_" + std::to_string(n));
        }
    }

    void emitMain(std::ostringstream &out)
    {
        out << "int main(void)\n{\n";
        out << "    tf_runtime_init(" << cString(options_.sourcePath) << ");\n";
        for (const auto &v : module_.globals)
        {
            if (v->rank > 0 && !v->isDynamic && v->type != Type::Error)
            {
                line(out, v->location);
                out << "    " << arrayInit(*v, "&g_" + v->name) << '\n';
            }
            else if (v->rank == 0 && v->type == Type::Record && !v->isConst)
            {
                line(out, v->location);
                out << "    t_" << v->typeDef->name << "_init(&g_" << v->name << ");\n";
            }
            else if (v->autoNew)
            {
                line(out, v->location);
                out << "    g_" << v->name << " = tf_ref_new(" << opsOf(*v->typeDef) << ");\n";
            }
        }

        const ControlDef *form = module_.form.get();
        if (form == nullptr)
        {
            out << "    p_Main();\n"
                << (checks_ ? "    tf_check_uncaught();\n" : "") << "    return 0;\n}\n";
            return;
        }

        line(out, form->location);
        out << "    c_" << form->name << " = tf_form_new(" << cString(form->name) << ");\n";
        emitSettings(out, *form);

        for (const auto &child : form->children)
        {
            const catalog::ControlType *type = catalog::findType(child->typeName);
            line(out, child->location);
            out << "    c_" << child->name << " = tf_control_new(c_" << form->name << ", "
                << type->cName << ", " << cString(child->name) << ");\n";
            emitSettings(out, *child);
        }

        // Event wiring, in the order the Subs appear in the source.
        const Procedure *load = nullptr;
        for (const auto &proc : module_.procedures)
        {
            if (proc->eventControl == nullptr)
            {
                continue;
            }
            if (proc->event == EventKind::Load)
            {
                load = proc.get();
                continue;
            }
            const catalog::ControlType *type = catalog::findType(proc->eventControl->typeName);
            for (const catalog::Event &event : type->events)
            {
                if (event.kind == proc->event && event.cName != nullptr)
                {
                    out << "    tf_control_on(c_" << proc->eventControl->name << ", " << event.cName
                        << ", p_" << proc->name << ");\n";
                }
            }
        }
        if (load != nullptr)
        {
            out << "    p_" << load->name << "();\n";
            if (checks_)
            {
                out << "    tf_check_uncaught();\n";
            }
        }
        out << "    return tf_form_run(c_" << form->name << ");\n}\n";
    }

    void emitSettings(std::ostringstream &out, const ControlDef &control)
    {
        const catalog::ControlType *type = catalog::findType(control.typeName);
        for (const PropertySetting &s : control.properties)
        {
            const catalog::Property *prop = catalog::findProperty(*type, s.name);
            line(out, s.location);
            if (prop->type == Type::String)
            {
                out << "    tf_set_str(c_" << control.name << ", " << prop->cName << ", "
                    << literal(s.value->text) << ");\n";
            }
            else
            {
                out << "    tf_set_int(c_" << control.name << ", " << prop->cName << ", "
                    << s.value->integer << ");\n";
            }
        }
    }

    // -- Statements --------------------------------------------------------

    void emitBlock(std::ostringstream &out, const Block &block, int depth)
    {
        for (const auto &stmt : block)
        {
            if (stmt != nullptr)
            {
                emitStatement(out, *stmt, depth);
            }
        }
    }

    void emitStatement(std::ostringstream &out, const Stmt &s, int depth)
    {
        switch (s.kind)
        {
            case StmtKind::Dim:
                // Locals are declared at the top of the procedure; inside a
                // loop each pass starts them afresh.
                // Dim r As New Customer makes the record here, each time the
                // Dim runs.
                if (s.insideLoop || std::any_of(s.declared.begin(), s.declared.end(),
                                                [](const Variable *v) { return v->autoNew; }))
                {
                    std::string code;
                    for (const Variable *v : s.declared)
                    {
                        const std::string n = names_.at(v);
                        std::string one;
                        if (v->autoNew)
                        {
                            one = n + " = tf_ref_replace(" + n + ", tf_ref_new(" +
                                  opsOf(*v->typeDef) + "));";
                        }
                        else if (!s.insideLoop)
                        {
                            continue;
                        }
                        else if (v->rank > 0)
                        {
                            one = "tf_array_erase(&" + n + ");";
                        }
                        else if (v->type == Type::Record)
                        {
                            one = "t_" + v->typeDef->name + "_free(&" + n + "); t_" +
                                  v->typeDef->name + "_init(&" + n + ");";
                        }
                        else if (v->type == Type::Ref)
                        {
                            one = n + " = tf_ref_replace(" + n + ", NULL);";
                        }
                        else
                        {
                            one = v->type == Type::String ? "tf_str_assign(&" + n + ", NULL);"
                                                          : n + " = 0;";
                        }
                        code += (code.empty() ? "" : " ") + one;
                    }
                    emitLine(out, s.location, depth, code);
                }
                return;
            case StmtKind::If:
                emitIf(out, s, 0, depth);
                return;
            case StmtKind::For:
                if (s.isForEach)
                {
                    emitBlock(out, s.setup, depth); // lowered by the semantic pass
                    emitDo(out, s, depth);
                    return;
                }
                emitFor(out, s, depth);
                return;
            case StmtKind::Do:
                emitDo(out, s, depth);
                return;
            case StmtKind::Try:
                emitTry(out, s, depth);
                return;
            case StmtKind::With:
                emitBlock(out, s.body, depth); // its target's setup, then its body
                return;
            case StmtKind::Const:
                return; // worked out by the compiler; uses are replaced by the value
            default:
                break;
        }

        StatementScope scope;
        prepare(scope, {s.value.get()});
        for (const auto &arg : s.args)
        {
            prepare(scope, {arg.get()});
        }
        std::string code;
        switch (s.kind)
        {
            case StmtKind::Assign:
                code = wrap(scope, assignment(s, scope));
                // An element or whole-array store fails in the store itself.
                if (checks_ && storeMayFail(*s.target))
                {
                    code += " if (tf_error_pending) " + failJump();
                }
                break;
            case StmtKind::ReDim:
                code = wrap(scope, reDim(s, scope));
                code += checks_ ? " if (tf_error_pending) " + failJump() : "";
                break;
            case StmtKind::Erase:
                for (const auto &name : s.args)
                {
                    code += (code.empty() ? "" : " ") + std::string("tf_array_erase(") +
                            wholeArrayRef(*name, scope) + ");";
                }
                code += checks_ ? " if (tf_error_pending) " + failJump() : "";
                break;
            case StmtKind::Call:
                code = wrap(scope, callStatement(s, scope));
                if (checks_)
                {
                    code += " if (tf_error_pending) " + failJump(); // the callee failed
                }
                break;
            case StmtKind::Exit:
                code = leave(s.exitLoop);
                break;
            case StmtKind::Return:
                code = returnStatement(s, scope);
                break;
            case StmtKind::Throw:
                code = throwStatement(s, scope);
                break;
            case StmtKind::ErrClear:
                code = "tf_err_clear();";
                break;
            case StmtKind::End:
                code = "tf_end(0);";
                break;
            default:
                break;
        }
        emitLine(out, s.location, depth, code);
    }

    // One BASIC statement: its #line, TF_LINE and C code on one line.
    void emitLine(std::ostringstream &out, Location where, int depth, const std::string &code)
    {
        line(out, where);
        out << indent(depth) << "TF_LINE(" << where.line << "); " << code << '\n';
    }

    // Emits arm `index` of an If (and, through else, the arms after it).
    void emitIf(std::ostringstream &out, const Stmt &s, std::size_t index, int depth)
    {
        const IfArm &arm = s.arms[index];
        StatementScope scope;
        prepare(scope, {arm.condition.get()});
        const std::string condition = unparenthesised(expr(*arm.condition, scope).code);

        line(out, arm.condition->location);
        if (scope.prelude.empty() && !(checks_ && mayFail(*arm.condition)))
        {
            out << indent(depth) << "TF_LINE(" << arm.condition->location.line << "); if ("
                << condition << ")\n";
            emitArmBodies(out, s, index, depth);
            return;
        }

        // A condition with temporaries: evaluate, release, then branch.
        // Kept on one line: a second line would be attributed to the next
        // BASIC line by the debugger.
        const std::string cond = "cond_" + std::to_string(arm.condition->location.line);
        out << indent(depth) << "{ TF_LINE(" << arm.condition->location.line << ");"
            << evaluate(scope, cond, condition, mayFail(*arm.condition)) << " if (" << cond
            << ")\n";
        emitArmBodies(out, s, index, depth);
        out << indent(depth) << "}\n";
    }

    // Drops one pair of parentheses around a whole condition, so "If a = b"
    // gives "if (a == b)": compilers warn about "if ((a == b))".
    static std::string unparenthesised(const std::string &code)
    {
        if (code.size() < 2 || code.front() != '(' || code.back() != ')')
        {
            return code;
        }
        int depth = 0;
        for (std::size_t i = 0; i + 1 < code.size(); ++i)
        {
            depth += code[i] == '(' ? 1 : code[i] == ')' ? -1 : 0;
            if (depth == 0)
            {
                return code; // the first '(' closes before the end: "(a) & (b)"
            }
        }
        return code.substr(1, code.size() - 2);
    }

    // " prelude; const int cond = (condition); releases;" for a condition
    // that needs temporaries.
    std::string evaluate(const StatementScope &scope, const std::string &name,
                         const std::string &condition, bool canFail = false)
    {
        std::string out = withTemporaries(scope, "const int " + name + " = (" + condition + ");");
        if (checks_ && canFail)
        {
            out += " if (tf_error_pending) " + failJump();
        }
        return out;
    }

    void emitArmBodies(std::ostringstream &out, const Stmt &s, std::size_t index, int depth)
    {
        out << indent(depth) << "{\n";
        emitBlock(out, s.arms[index].body, depth + 1);
        out << indent(depth) << "}\n";

        if (index + 1 < s.arms.size())
        {
            out << indent(depth) << "else\n" << indent(depth) << "{\n";
            emitIf(out, s, index + 1, depth + 1);
            out << indent(depth) << "}\n";
        }
        else if (s.hasElse)
        {
            out << indent(depth) << "else\n" << indent(depth) << "{\n";
            emitBlock(out, s.elseBody, depth + 1);
            out << indent(depth) << "}\n";
        }
    }

    // -- Loops -------------------------------------------------------------

    // For v = start To limit [Step step], with VB semantics: limit and step
    // are evaluated once, before the first pass; the loop runs while v <=
    // limit (v >= limit for a negative step); Next adds step with the usual
    // overflow check, so a loop that ends at the largest Integer overflows,
    // as in VB.
    void emitFor(std::ostringstream &out, const Stmt &s, int depth)
    {
        const int id = enterLoop(s);
        const std::string var = variableValue(*s.target->variable);
        const Type varType = storage(*s.target->variable);
        StatementScope scope;
        prepare(scope, {s.value.get(), s.limit.get(), s.step.get()});

        // Literal limits and steps are used directly; anything else is
        // evaluated once into to_<id> / step_<id>, after the start value.
        const bool limitConst = isConstant(*s.limit);
        const bool stepConst = s.step == nullptr || isConstant(*s.step);
        std::string code;
        std::string start = settle(*s.value, forOperand(*s.value, scope), scope);
        if (!isConstant(*s.value) && (!limitConst || !stepConst))
        {
            code += "const " + declaration(varType, "from_" + std::to_string(id)) + " = " + start +
                    "; ";
            start = "from_" + std::to_string(id);
        }
        std::string limit = settle(*s.limit, forOperand(*s.limit, scope), scope);
        if (!limitConst)
        {
            code +=
                "const " + declaration(varType, "to_" + std::to_string(id)) + " = " + limit + "; ";
            limit = "to_" + std::to_string(id);
        }
        std::string step =
            s.step == nullptr ? "1" : settle(*s.step, forOperand(*s.step, scope), scope);
        if (!stepConst)
        {
            code +=
                "const " + declaration(varType, "step_" + std::to_string(id)) + " = " + step + "; ";
            step = "step_" + std::to_string(id);
        }
        code += var + " = " + start + ";";

        // Temporaries need a block of their own; the loop goes inside it.
        const bool braced = !scope.prelude.empty();
        line(out, s.location);
        out << indent(depth) << (braced ? "{ " : "") << "TF_LINE(" << s.location.line << ");"
            << withTemporaries(scope, code) << '\n';

        const int inner = depth + (braced ? 1 : 0);
        std::string test;
        if (stepConst)
        {
            const bool down = s.step != nullptr && isNegativeConstant(*s.step);
            test = var + (down ? " < " : " > ") + limit;
        }
        else
        {
            test = step + " >= 0 ? " + var + " > " + limit + " : " + var + " < " + limit;
        }
        out << indent(inner) << "for (;;)\n" << indent(inner) << "{\n";
        emitLine(out, s.location, inner + 1, "if (" + test + ") break;");
        emitBlock(out, s.body, inner + 1);
        const std::string next = opPrefix(varType) + "add(" + var + ", " + step + ")";
        emitLine(out, s.nextLocation, inner + 1,
                 checks_ ? "{ const " + declaration(varType, "next") + " = " + next +
                               "; if (tf_error_pending) " + failJump() + " " + var + " = next; }"
                         : var + " = " + next + ";");
        out << indent(inner) << "}\n";
        leaveLoop(out, s, inner);
        if (braced)
        {
            out << indent(depth) << "}\n";
        }
    }

    // Do [While|Until c] ... Loop [While|Until c], and While ... Wend.
    void emitDo(std::ostringstream &out, const Stmt &s, int depth)
    {
        enterLoop(s);
        line(out, s.location);
        out << indent(depth) << "for (;;)\n" << indent(depth) << "{\n";
        if (s.testAtTop)
        {
            emitLoopTest(out, s, s.location, depth + 1);
        }
        emitBlock(out, s.body, depth + 1);
        if (s.condition != nullptr && !s.testAtTop)
        {
            emitLoopTest(out, s, s.loopLocation, depth + 1);
        }
        out << indent(depth) << "}\n";
        leaveLoop(out, s, depth);
    }

    // "if (!(cond)) break;" for While, "if (cond) break;" for Until.
    void emitLoopTest(std::ostringstream &out, const Stmt &s, Location where, int depth)
    {
        StatementScope scope;
        prepare(scope, {s.condition.get()});
        const std::string condition = unparenthesised(expr(*s.condition, scope).code);
        if (scope.prelude.empty() && !(checks_ && mayFail(*s.condition)))
        {
            emitLine(out, where, depth,
                     std::string("if (") + (s.until ? "" : "!") + "(" + condition + ")) break;");
            return;
        }
        const std::string cond = "cond_" + std::to_string(where.line);
        line(out, where);
        out << indent(depth) << "{ TF_LINE(" << where.line << ");"
            << evaluate(scope, cond, condition, mayFail(*s.condition)) << " if ("
            << (s.until ? "" : "!") << cond << ") break; }\n";
    }

    int enterLoop(const Stmt &s)
    {
        loopIds_[&s] = ++loopCount_;
        loopStack_.push_back(&s);
        return loopCount_;
    }

    // Closes a loop: the label an Exit from an inner loop jumps to, if any.
    void leaveLoop(std::ostringstream &out, const Stmt &s, int depth)
    {
        loopStack_.pop_back();
        if (loopLabels_.count(&s) != 0)
        {
            out << indent(depth) << "exit_" << loopIds_.at(&s) << ":;\n";
        }
    }

    // Exit For / Do / While: break out of the innermost C loop, or jump past
    // an outer one.
    std::string exitLoop(const Stmt &loop)
    {
        if (!loopStack_.empty() && loopStack_.back() == &loop)
        {
            return "break;";
        }
        loopLabels_.insert(&loop);
        return "goto exit_" + std::to_string(loopIds_.at(&loop)) + ";";
    }

    // Exit Sub / Exit Function / Return without a value.
    std::string leaveProcedure()
    {
        if (!cleanup_.empty())
        {
            usesCleanupLabel_ = true;
            return "goto done;";
        }
        return current_->isFunction ? "return r_" + current_->name + ";" : "return;";
    }

    std::string returnStatement(const Stmt &s, StatementScope &scope)
    {
        if (s.value == nullptr)
        {
            return leave(nullptr);
        }
        const std::string result = "r_" + current_->name;
        const Value v = settle(*s.value, expr(*s.value, scope), scope);
        const Variable &r = *current_->result;
        std::string store;
        if (r.type == Type::Record)
        {
            store = "t_" + r.typeDef->name + "_copy(&" + result + ", &" +
                    parenthesised(recordValue(*s.value, v, scope)) + ");";
        }
        else if (r.type == Type::Ref)
        {
            store = result + " = tf_ref_replace(" + result + ", " +
                    ownedRef(*s.value, r.typeDef, v) + ");";
        }
        else if (r.type == Type::String)
        {
            store = "tf_str_assign(&" + result + ", " + ownedCode(v) + ");";
        }
        else
        {
            store = result + " = " + v.code + ";";
        }
        return wrap(scope, store) + " " + leave(nullptr);
    }

    // A For bound: a literal (possibly negated) is written as a plain number.
    std::string forOperand(const Expr &e, StatementScope &scope)
    {
        if (!isConstant(e))
        {
            return expr(e, scope).code;
        }
        const bool negated = e.kind == ExprKind::Unary;
        const Expr &literal = negated ? *e.left : e;
        if (isReal(literal))
        {
            return doubleLiteral(negated ? -literal.real : literal.real);
        }
        return integerLiteral(negated ? -literal.integer : literal.integer);
    }

    // A number known when compiling: a literal, or a numeric Const or Enum member.
    static bool isLiteralNumber(const Expr &e)
    {
        return e.kind == ExprKind::IntegerLiteral || e.kind == ExprKind::DoubleLiteral ||
               (e.kind == ExprKind::Constant && e.type != Type::String && e.type != Type::Boolean);
    }

    static bool isReal(const Expr &literal)
    {
        return literal.kind == ExprKind::DoubleLiteral ||
               (literal.kind == ExprKind::Constant && literal.type == Type::Double);
    }

    static bool isConstant(const Expr &e)
    {
        return isLiteralNumber(e) ||
               (e.kind == ExprKind::Unary && e.text == "-" && isLiteralNumber(*e.left));
    }

    static bool isNegativeConstant(const Expr &e)
    {
        const bool negated = e.kind == ExprKind::Unary;
        const Expr &literal = negated ? *e.left : e;
        const bool negative = isReal(literal) ? literal.real < 0 : literal.integer < 0;
        return negated != negative && (literal.real != 0 || literal.integer != 0);
    }

    // -- Errors ------------------------------------------------------------

    // Throw [number [, description]] and Err.Raise number [, source [, desc]].
    std::string throwStatement(const Stmt &s, StatementScope &scope)
    {
        std::string call;
        if (s.args.empty())
        {
            call = "tf_rethrow();";
        }
        else
        {
            std::vector<std::string> parts = {"0", "NULL", "NULL"}; // number, source, description
            for (std::size_t i = 0; i < s.args.size(); ++i)
            {
                const std::size_t slot = s.viaErrRaise || i == 0 ? i : 2;
                const Expr &arg = *s.args[i];
                parts[slot] = i == 0 ? settle(arg, expr(arg, scope), scope).code
                                     : borrowed(settle(arg, expr(arg, scope), scope), scope).code;
            }
            call = "tf_throw(" + parts[0] + ", " + parts[1] + ", " + parts[2] + ");";
        }
        // The error is now pending (or the program has ended): go to the handler.
        return checks_ ? wrap(scope, call) + " " + failJump() : wrap(scope, call);
    }

    // Try ... Catch [When c] ... Finally ... End Try. See the file comment
    // for the labels; everything is inside one C block so the names of the
    // Try's variables (leave_<n>, saved_<n>) stay local to it.
    void emitTry(std::ostringstream &out, const Stmt &s, int depth)
    {
        const int id = ++tryCount_;
        const std::string n = std::to_string(id);
        TryContext context;
        context.id = id;
        context.hasCatches = !s.catches.empty();
        context.loopFloor = loopStack_.size();
        tries_.push_back(context);

        std::ostringstream body;
        emitBlock(body, s.body, depth + 1);
        if (!endsInJump(s.body))
        {
            body << indent(depth + 1) << "goto finally_" << n << ";\n";
        }

        std::ostringstream catches;
        tries_.back().part = TryContext::Part::Catch;
        if (tries_.back().failUsed)
        {
            catches << indent(depth) << "fail_" << n << ":\n";
            catches << indent(depth + 1) << "tf_err_catch();\n";
            bool catchesAll = false;
            for (const CatchClause &c : s.catches)
            {
                emitCatch(catches, c, n, depth + 1);
                if (c.when == nullptr)
                {
                    catchesAll = true;
                    break; // later clauses could never run
                }
            }
            if (!catchesAll)
            {
                catches << indent(depth + 1) << "tf_rethrow(); /* no Catch matched */\n";
            }
        }

        std::ostringstream finally;
        tries_.back().part = TryContext::Part::Finally;
        finally << indent(depth) << "finally_" << n << ":\n";
        line(finally, s.hasFinally ? s.finallyLocation : s.endLocation);
        const int finallyLine = (s.hasFinally ? s.finallyLocation : s.endLocation).line;
        finally << indent(depth + 1) << "TF_LINE(" << finallyLine << "); tf_try_leave();"
                << (s.hasFinally ? " tf_err_save(&saved_" + n + ");" : "") << '\n';
        emitBlock(finally, s.finallyBody, depth + 1);

        // Back in the surrounding code: pass on a pending error, or take the
        // way out that ran the Finally.
        TryContext done = tries_.back();
        tries_.pop_back();
        std::string after = s.hasFinally ? "tf_err_restore(&saved_" + n + "); " : "";
        after += "if (tf_error_pending) " + failJump();
        for (std::size_t i = 0; i < done.leaves.size(); ++i)
        {
            after +=
                " if (leave_" + n + " == " + std::to_string(i + 1) + ") " + leave(done.leaves[i]);
        }
        emitLine(finally, s.endLocation, depth + 1, after);
        if (done.finallyFailUsed)
        {
            // An error in the Finally replaces the one it set aside.
            finally << indent(depth + 1) << "goto end_" << n << ";\n";
            finally << indent(depth) << "ffail_" << n << ":\n";
            finally << indent(depth + 1) << "tf_err_discard(&saved_" << n << "); " << failJump()
                    << '\n';
            finally << indent(depth) << "end_" << n << ":;\n";
        }

        line(out, s.location);
        out << indent(depth) << "{ TF_LINE(" << s.location.line << "); tf_try_enter();"
            << (done.leaves.empty() ? "" : " int leave_" + n + " = 0;")
            << (s.hasFinally ? " tf_saved_error saved_" + n + ";" : "") << '\n';
        out << body.str() << catches.str() << finally.str();
        out << indent(depth) << "}\n";
    }

    // One Catch: "if (When) { body; goto finally; }", or the body alone.
    void emitCatch(std::ostringstream &out, const CatchClause &c, const std::string &n, int depth)
    {
        const std::string toFinally = "goto finally_" + n + ";";
        if (c.when == nullptr)
        {
            line(out, c.location);
            out << indent(depth) << "TF_LINE(" << c.location.line << ");\n";
            out << indent(depth) << "{\n";
            emitBlock(out, c.body, depth + 1);
            if (!endsInJump(c.body))
            {
                out << indent(depth + 1) << toFinally << '\n';
            }
            out << indent(depth) << "}\n";
            return;
        }
        StatementScope scope;
        prepare(scope, {c.when.get()});
        const std::string condition = unparenthesised(expr(*c.when, scope).code);
        const std::string cond = "cond_" + std::to_string(c.location.line);
        line(out, c.location);
        out << indent(depth) << "{ TF_LINE(" << c.location.line << ");"
            << evaluate(scope, cond, condition, mayFail(*c.when)) << " if (" << cond << ")\n";
        out << indent(depth) << "{\n";
        emitBlock(out, c.body, depth + 1);
        if (!endsInJump(c.body))
        {
            out << indent(depth + 1) << toFinally << '\n';
        }
        out << indent(depth) << "}\n";
        out << indent(depth) << "}\n";
    }

    // Does the block end with a statement that always jumps away? Then the
    // jump to the Finally after it would be dead code (compilers warn).
    static bool endsInJump(const Block &block)
    {
        if (block.empty() || block.back() == nullptr)
        {
            return false;
        }
        const StmtKind k = block.back()->kind;
        return k == StmtKind::Throw || k == StmtKind::Exit || k == StmtKind::Return ||
               k == StmtKind::End;
    }

    // Where an error goes from here: the innermost Try part's handler, or the
    // procedure's cleanup, which returns to the caller.
    std::string failTarget()
    {
        if (tries_.empty())
        {
            usesCleanupLabel_ = true;
            return "done";
        }
        TryContext &t = tries_.back();
        const std::string n = std::to_string(t.id);
        switch (t.part)
        {
            case TryContext::Part::Body:
                if (!t.hasCatches)
                {
                    return "finally_" + n;
                }
                t.failUsed = true;
                return "fail_" + n;
            case TryContext::Part::Catch:
                return "finally_" + n; // the Finally runs, then the error goes on
            case TryContext::Part::Finally:
                t.finallyFailUsed = true;
                return "ffail_" + n;
        }
        return "done";
    }

    std::string failJump() { return "goto " + failTarget() + ";"; }

    // After something that may have failed: on an error, release the
    // statement's string temporaries so far and jump to the handler.
    void checkpoint(StatementScope &scope)
    {
        if (!checks_)
        {
            return;
        }
        std::string releases;
        for (const std::string &r : scope.releases)
        {
            releases += r + " ";
        }
        scope.prelude.push_back(releases.empty()
                                    ? "if (tf_error_pending) " + failJump()
                                    : "if (tf_error_pending) { " + releases + failJump() + " }");
    }

    // A value that can fail is computed into a temporary and checked before
    // anything uses it, so a failed statement changes nothing.
    Value settle(const Expr &e, const Value &v, StatementScope &scope)
    {
        if (!checks_ || !mayFail(e))
        {
            return v;
        }
        const std::string temp = scope.newTemp();
        if (e.type == Type::Record) // a record: park its address, not a copy
        {
            scope.prelude.push_back(cTypeOf(e) + " *" + temp + " = &" + parenthesised(v.code) +
                                    ";");
            checkpoint(scope);
            return {"(*" + temp + ")", false};
        }
        scope.prelude.push_back(declarationOf(e, temp) + " = " + v.code + ";");
        if (v.owned)
        {
            scope.releases.push_back("tf_str_release(" + temp + ");");
        }
        checkpoint(scope);
        return {temp, false};
    }

    std::string settle(const Expr &e, const std::string &code, StatementScope &scope)
    {
        return settle(e, Value{code, false}, scope).code;
    }

    // Can evaluating e raise an error, apart from the Functions it calls
    // (which are checked where they are hoisted)? Arithmetic can overflow
    // or divide by zero, conversions can fail, and joining strings can make
    // one too long (error 14); comparisons and logic cannot (running out of
    // memory ends the program).
    static bool mayFail(const Expr &e)
    {
        for (const auto &arg : e.args)
        {
            if (mayFail(*arg))
            {
                return true;
            }
        }
        if ((e.left != nullptr && mayFail(*e.left)) || (e.right != nullptr && mayFail(*e.right)))
        {
            return true;
        }
        switch (e.kind)
        {
            case ExprKind::Index:
                return true; // an index out of range
            case ExprKind::Unary:
                return e.text == "-" && (e.type == Type::Integer || e.type == Type::Long);
            case ExprKind::Binary:
                return isNumeric(e.type) || e.type == Type::String; // arithmetic, & and +
            case ExprKind::Member:
                return e.field != nullptr && e.left->type == Type::Ref; // Nothing: error 91
            case ExprKind::Call:
                switch (e.builtin)
                {
                    case Builtin::CInt:
                        return storage(*e.args[0]) != Type::Integer;
                    case Builtin::CLng:
                        return storage(*e.args[0]) == Type::Double ||
                               storage(*e.args[0]) == Type::String;
                    case Builtin::CDbl:
                        return e.args[0]->type == Type::String;
                    case Builtin::Val:
                        return true; // a number too large for a Double
                    case Builtin::LBound:
                    case Builtin::UBound:
                        return true; // an empty array, a dimension it does not have
                    case Builtin::Count:
                        return mayFail(*e.args[0]); // a Ref on the way that is Nothing
                    default:
                        return false;
                }
            default:
                return false;
        }
    }

    // Exit For/Do/While (loop) or Exit Sub/Function and Return (null).
    // Leaving through a Try runs its Finally first: record the way out in
    // leave_<n> and jump there; the Finally's end takes it, from outside.
    std::string leave(const Stmt *loop)
    {
        if (!tries_.empty())
        {
            TryContext &t = tries_.back();
            const bool crosses = loop == nullptr || loopIndex(*loop) < t.loopFloor;
            if (crosses)
            {
                t.leaves.push_back(loop);
                const std::string n = std::to_string(t.id);
                return "leave_" + n + " = " + std::to_string(t.leaves.size()) + "; goto finally_" +
                       n + ";";
            }
        }
        return loop != nullptr ? exitLoop(*loop) : leaveProcedure();
    }

    std::size_t loopIndex(const Stmt &loop) const
    {
        for (std::size_t i = 0; i < loopStack_.size(); ++i)
        {
            if (loopStack_[i] == &loop)
            {
                return i;
            }
        }
        return 0;
    }

    static bool containsTry(const Module &module)
    {
        for (const auto &proc : module.procedures)
        {
            if (containsTry(proc->body))
            {
                return true;
            }
        }
        return false;
    }

    static bool containsTry(const Block &block)
    {
        for (const auto &s : block)
        {
            if (s == nullptr)
            {
                continue;
            }
            if (s->kind == StmtKind::Try || containsTry(s->body) || containsTry(s->elseBody))
            {
                return true;
            }
            for (const IfArm &arm : s->arms)
            {
                if (containsTry(arm.body))
                {
                    return true;
                }
            }
        }
        return false;
    }

    static std::string lowerCase(std::string text)
    {
        for (char &c : text)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return text;
    }

    // -- Assignments and calls ---------------------------------------------

    std::string assignment(const Stmt &s, StatementScope &scope)
    {
        const Expr &target = *s.target;

        if (target.rank > 0) // b = a, o.Lines = other: copy the whole array
        {
            const std::string to = wholeArrayRef(target, scope);
            return "tf_array_copy(" + to + ", " + wholeArrayRef(*s.value, scope) + ");";
        }
        if (target.kind == ExprKind::Index ||
            (target.kind == ExprKind::Member && target.field != nullptr))
        {
            // The target's indexes first (left to right), then the value; any
            // element's address is taken last, after a call that might ReDim.
            if (target.type == Type::Record || target.type == Type::String ||
                (target.kind == ExprKind::Index && target.type != Type::Ref))
            {
                const std::string at = address(target, scope);
                const Value v = settle(*s.value, expr(*s.value, scope), scope);
                if (target.type == Type::Record)
                {
                    return copyRecord(target, at, recordValue(*s.value, v, scope));
                }
                if (target.type == Type::String)
                {
                    return "tf_str_assign(" + at + ", " + ownedCode(v) + ");";
                }
                return "*" + at + " = " + v.code + ";";
            }
            const std::string to = lvalue(target, scope);
            const Value v = settle(*s.value, expr(*s.value, scope), scope);
            if (target.type == Type::Ref)
            {
                return to + " = tf_ref_replace(" + to + ", " +
                       ownedRef(*s.value, target.typeDef, v) + ");";
            }
            return to + " = " + v.code + ";";
        }

        if (target.kind == ExprKind::Member)
        {
            const ControlDef *control = target.left->control;
            const catalog::ControlType *type = catalog::findType(control->typeName);
            const catalog::Property *prop = catalog::findProperty(*type, target.text);
            if (prop->type == Type::String)
            {
                // The setter borrows, so an owned value goes through a temporary.
                const Value v = borrowed(settle(*s.value, expr(*s.value, scope), scope), scope);
                return "tf_set_str(c_" + control->name + ", " + prop->cName + ", " + v.code + ");";
            }
            return "tf_set_int(c_" + control->name + ", " + prop->cName + ", " +
                   settle(*s.value, expr(*s.value, scope), scope).code + ");";
        }

        const Variable &var = *target.variable;
        const Value v = settle(*s.value, expr(*s.value, scope), scope);
        if (target.type == Type::Record)
        {
            return copyRecord(target, variableAddress(var), recordValue(*s.value, v, scope));
        }
        if (target.type == Type::Ref)
        {
            const std::string to = variableValue(var);
            return to + " = tf_ref_replace(" + to + ", " + ownedRef(*s.value, target.typeDef, v) +
                   ");";
        }
        if (target.type == Type::String)
        {
            return "tf_str_assign(" + variableAddress(var) + ", " + ownedCode(v) + ");";
        }
        return variableValue(var) + " = " + v.code + ";";
    }

    // to = value for records: a deep copy (an owned value, a Function's
    // result, is freed after the statement like any temporary).
    std::string copyRecord(const Expr &target, const std::string &to, const std::string &from)
    {
        return "t_" + target.typeDef->name + "_copy(" + to + ", &" + parenthesised(from) + ");";
    }

    // The record a Ref refers to, for a member or a copy: (*(t_X *)tf_ref_get(r, ops)).
    std::string deref(const Expr &ref, const std::string &code)
    {
        return "(*(t_" + ref.typeDef->name + " *)tf_ref_get(" + code + ", " + opsOf(*ref.typeDef) +
               "))";
    }

    // A record value for copying: a Ref's record is looked up first and,
    // inside a Try, checked (Nothing is error 91) before anything is copied.
    std::string recordValue(const Expr &value, const Value &v, StatementScope &scope)
    {
        if (value.type != Type::Ref)
        {
            return v.code;
        }
        const std::string temp = scope.newTemp();
        scope.prelude.push_back("t_" + value.typeDef->name + " *" + temp + " = tf_ref_get(" +
                                v.code + ", " + opsOf(*value.typeDef) + ");");
        checkpoint(scope);
        return "(*" + temp + ")";
    }

    // A Ref value to store (+1): a value record becomes a new copy; Nothing is NULL.
    std::string ownedRef(const Expr &value, const TypeDef *def, const Value &v)
    {
        if (value.type == Type::Record)
        {
            return "tf_ref_new_copy(" + opsOf(*def) + ", &" + parenthesised(v.code) + ")";
        }
        if (value.kind == ExprKind::Nothing)
        {
            return "NULL";
        }
        return v.owned ? v.code : "tf_ref_ref(" + v.code + ")";
    }

    // A Ref read for a moment (Is, a ByVal argument): an owned one (New, a
    // Function's result) is parked and released after the statement.
    Value heldRef(const Expr &e, const Value &v, StatementScope &scope)
    {
        if (!v.owned)
        {
            return v;
        }
        const std::string temp = scope.newTemp();
        scope.prelude.push_back(cTypeOf(e) + temp + " = " + v.code + ";");
        scope.releases.push_back("tf_ref_release(" + temp + ");");
        return {temp, false};
    }

    // Does storing into this target do work that can fail: find an element,
    // copy an array or a record (which may be locked)?
    static bool storeMayFail(const Expr &target)
    {
        if (target.rank > 0 || target.type == Type::Record || target.kind == ExprKind::Index)
        {
            return true;
        }
        // A member of a record variable is stored straight into it; through
        // a Ref (Nothing) or an element (an index) the store can fail.
        return target.kind == ExprKind::Member && target.field != nullptr &&
               (target.left->type == Type::Ref ||
                (target.left->kind != ExprKind::Name && storeMayFail(*target.left)));
    }

    // -- Paths: variables, elements, members -------------------------------------

    // The C lvalue of a variable, an element or a member path, as stored to
    // (never snapshotted). arrays, if given, collects the arrays the path's
    // elements live in, for locking around a call that receives it ByRef.
    std::string lvalue(const Expr &e, StatementScope &scope,
                       std::vector<std::string> *arrays = nullptr)
    {
        switch (e.kind)
        {
            case ExprKind::Name:
                return variableValue(*e.variable);
            case ExprKind::Index:
                return "(*" + address(e, scope, arrays) + ")";
            case ExprKind::Member:
                if (e.left->type == Type::Ref)
                {
                    std::string ref = expr(*e.left, scope).code;
                    if (arrays != nullptr)
                    {
                        // Passed ByRef: hold the record until the statement
                        // ends, so the callee cannot free it from under itself.
                        const std::string held = scope.newTemp();
                        scope.prelude.push_back(cTypeOf(*e.left) + held + " = tf_ref_ref(" + ref +
                                                ");");
                        scope.releases.push_back("tf_ref_release(" + held + ");");
                        ref = held;
                    }
                    return deref(*e.left, ref) + ".f_" + e.text;
                }
                return parenthesised(lvalue(*e.left, scope, arrays)) + ".f_" + e.text;
            default:
                return expr(e, scope).code; // a Function's result, read through
        }
    }

    // The address of a path: &v_c, (int32_t *)tf_array_at1(&v_a, i), &v_o.f_Name.
    std::string address(const Expr &e, StatementScope &scope,
                        std::vector<std::string> *arrays = nullptr)
    {
        switch (e.kind)
        {
            case ExprKind::Name:
                return variableAddress(*e.variable);
            case ExprKind::Index:
            {
                const std::string ref = elementArray(e, scope, arrays);
                if (arrays != nullptr)
                {
                    arrays->push_back(ref);
                }
                return pointerCastOf(e) + elementAt(e, ref, scope);
            }
            default:
                return "&" + parenthesised(lvalue(e, scope, arrays));
        }
    }

    // The tf_array * an element of e (a(i) or o.Lines(i)) is looked up in.
    std::string elementArray(const Expr &e, StatementScope &scope,
                             std::vector<std::string> *arrays = nullptr)
    {
        return e.left != nullptr ? "&" + parenthesised(lvalue(*e.left, scope, arrays))
                                 : arrayRef(*e.variable);
    }

    // A whole array (a Name or a record's array member) as a tf_array *.
    std::string wholeArrayRef(const Expr &e, StatementScope &scope)
    {
        return e.kind == ExprKind::Name ? arrayRef(*e.variable)
                                        : "&" + parenthesised(lvalue(e, scope));
    }

    // The variable a path starts from, or null (a Function's result).
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

    // "&v_a", "v_values": an array variable or parameter, not a path.
    static bool isSimpleRef(const std::string &ref)
    {
        const std::size_t start = !ref.empty() && ref[0] == '&' ? 1 : 0;
        return ref.size() > start &&
               std::all_of(ref.begin() + static_cast<std::ptrdiff_t>(start), ref.end(), [](char c) {
                   return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
               });
    }

    static bool pathHasIndex(const Expr &e)
    {
        if (e.kind == ExprKind::Index)
        {
            return true;
        }
        return e.kind == ExprKind::Member && e.field != nullptr && pathHasIndex(*e.left);
    }

    // Can a call later in the statement change what this path reads?
    static bool reachable(const Variable &v, const StatementScope &scope)
    {
        return v.isModuleLevel || v.byRef || (v.isParameter && v.rank > 0) ||
               scope.passedByRef.count(&v) != 0;
    }

    static std::string parenthesised(const std::string &code)
    {
        const bool simple =
            std::all_of(code.begin(), code.end(),
                        [](char c) {
                            return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' ||
                                   c == '.';
                        }) ||
            (code.size() > 2 && code.front() == '(' && unparenthesised(code) != code);
        return simple ? code : "(" + code + ")";
    }

    // Hand over an owned value; take a reference to a borrowed one.
    static std::string ownedCode(const Value &v)
    {
        return v.owned ? v.code : "tf_str_ref(" + v.code + ")";
    }

    // A call used as a statement; a Function's result is discarded.
    std::string callStatement(const Stmt &s, StatementScope &scope)
    {
        const Procedure &callee = *s.procedure;
        std::vector<std::string> locks;
        const std::string call =
            "p_" + callee.name + "(" + arguments(callee, s.args, scope, locks) + ")";
        if (callee.isFunction && callee.result->type == Type::Record)
        {
            // A record result is still the caller's to free.
            const std::string temp = scope.newTemp();
            scope.prelude.push_back(declarationOf(*callee.result, temp) + ";");
            scope.releases.push_back("t_" + callee.result->typeDef->name + "_free(&" + temp + ");");
            return lockedAround(locks, temp + " = " + call + ";");
        }
        const std::string statement =
            !callee.isFunction                    ? call + ";"
            : callee.result->type == Type::String ? "tf_str_release(" + call + ");"
            : callee.result->type == Type::Ref    ? "tf_ref_release(" + call + ");"
                                                  : "(void)" + call + ";";
        return lockedAround(locks, statement);
    }

    // ByRef arguments pass the variable's address; ByVal Strings are
    // borrowed for the duration of the call (the callee takes its own
    // reference).
    std::string arguments(const Procedure &callee, const std::vector<ExprPtr> &args,
                          StatementScope &scope, std::vector<std::string> &locks)
    {
        std::string out;
        for (std::size_t i = 0; i < args.size(); ++i)
        {
            const Variable &param = *callee.parameters[i];
            std::string code;
            if (param.rank > 0)
            {
                code = wholeArrayRef(*args[i], scope); // arrays always go by pointer
            }
            else if (param.byRef && pathHasIndex(*args[i]))
            {
                // An element (or a member of one) ByRef: find it, indexes
                // checked, before the call, and lock each array on the way
                // meanwhile so a ReDim cannot move it.
                std::vector<std::string> arrays;
                const std::string at = address(*args[i], scope, &arrays);
                for (const std::string &array : arrays)
                {
                    // A variable's array stays put; one inside an element is
                    // captured, so the unlock reaches the array just locked.
                    if (isSimpleRef(array))
                    {
                        locks.push_back(array);
                        continue;
                    }
                    const std::string held = scope.newTemp();
                    scope.prelude.push_back("tf_array *" + held + " = " + array + ";");
                    locks.push_back(held);
                }
                const std::string temp = scope.newTemp();
                const std::string cast = pointerCastOf(*args[i]); // "(int32_t *)"
                scope.prelude.push_back(cast.substr(1, cast.size() - 2) + temp + " = " + at + ";");
                checkpoint(scope);
                code = temp;
            }
            else if (param.byRef)
            {
                code = address(*args[i], scope);
            }
            else if (param.type == Type::Record)
            {
                // ByVal: the callee copies it; pass where it is (a Ref's record).
                const Value v = settle(*args[i], expr(*args[i], scope), scope);
                code = "&" + parenthesised(recordValue(*args[i], v, scope));
            }
            else if (param.type == Type::Ref)
            {
                // ByVal: borrowed for the call (the callee takes its own
                // reference); a value record becomes a new copy for it.
                const Value v = settle(*args[i], expr(*args[i], scope), scope);
                if (args[i]->type == Type::Record)
                {
                    const std::string temp = scope.newTemp();
                    scope.prelude.push_back(cTypeOf(param) + temp + " = " +
                                            ownedRef(*args[i], param.typeDef, v) + ";");
                    scope.releases.push_back("tf_ref_release(" + temp + ");");
                    code = temp;
                }
                else
                {
                    code = heldRef(*args[i], v, scope).code;
                }
            }
            else if (param.type == Type::String)
            {
                code = borrowed(settle(*args[i], expr(*args[i], scope), scope), scope).code;
            }
            else
            {
                code = settle(*args[i], expr(*args[i], scope), scope).code;
            }
            out += (i == 0 ? "" : ", ") + code;
        }
        return out;
    }

    // Wraps a statement with its temporaries: { prelude; code; releases; }
    static std::string wrap(const StatementScope &scope, const std::string &code)
    {
        if (scope.prelude.empty())
        {
            return code;
        }
        return "{" + withTemporaries(scope, code) + " }";
    }

    // " prelude... code releases..." with no braces around it.
    static std::string withTemporaries(const StatementScope &scope, const std::string &code)
    {
        std::string out;
        for (const std::string &p : scope.prelude)
        {
            out += ' ' + p;
        }
        out += ' ' + code;
        for (const std::string &r : scope.releases)
        {
            out += ' ' + r;
        }
        return out;
    }

    // -- Expressions -------------------------------------------------------

    // Records the Function calls the statement's expressions will make, in
    // the order they are evaluated. Must be called before expr() on them.
    static void prepare(StatementScope &scope, std::initializer_list<const Expr *> exprs)
    {
        for (const Expr *e : exprs)
        {
            if (e != nullptr)
            {
                countCalls(*e, scope);
            }
        }
    }

    static void countCalls(const Expr &e, StatementScope &scope)
    {
        if (e.kind == ExprKind::Call && e.procedure != nullptr)
        {
            ++scope.pendingCalls;
            for (std::size_t i = 0; i < e.args.size(); ++i)
            {
                if (e.procedure->parameters[i]->byRef && e.args[i]->variable != nullptr)
                {
                    scope.passedByRef.insert(e.args[i]->variable);
                }
            }
        }
        for (const auto &arg : e.args)
        {
            countCalls(*arg, scope);
        }
        if (e.left != nullptr)
        {
            countCalls(*e.left, scope);
        }
        if (e.right != nullptr)
        {
            countCalls(*e.right, scope);
        }
    }

    // A read that a later call in the statement could change is copied now.
    static Value snapshot(Type type, const std::string &code, StatementScope &scope)
    {
        const std::string temp = scope.newTemp();
        if (type == Type::String)
        {
            scope.prelude.push_back("tf_string *" + temp + " = tf_str_ref(" + code + ");");
            scope.releases.push_back("tf_str_release(" + temp + ");");
        }
        else
        {
            scope.prelude.push_back(declaration(type, temp) + " = " + code + ";");
        }
        return {temp, false};
    }

    Value expr(const Expr &e, StatementScope &scope)
    {
        switch (e.kind)
        {
            case ExprKind::IntegerLiteral:
                return {integerLiteral(e.integer), false};
            case ExprKind::DoubleLiteral:
                return {doubleLiteral(e.real), false};
            case ExprKind::Constant:
                return {constantCode(e), false};
            case ExprKind::New:
                return {"tf_ref_new(" + opsOf(*e.typeDef) + ")", true};
            case ExprKind::Nothing:
                return {"NULL", false};
            case ExprKind::StringLiteral:
                return {literal(e.text), false};
            case ExprKind::BooleanLiteral:
                return {e.integer != 0 ? "1" : "0", false};
            case ExprKind::Name:
            {
                const Variable &v = *e.variable;
                if (scope.pendingCalls > 0 && v.type == Type::Ref && reachable(v, scope))
                {
                    // Held, so a call that drops it cannot free it meanwhile.
                    const std::string temp = scope.newTemp();
                    scope.prelude.push_back(cTypeOf(v) + temp + " = tf_ref_ref(" +
                                            variableValue(v) + ");");
                    scope.releases.push_back("tf_ref_release(" + temp + ");");
                    return {temp, false};
                }
                if (scope.pendingCalls > 0 && v.type != Type::Record && reachable(v, scope))
                {
                    return snapshot(storage(v), variableValue(v), scope);
                }
                return {variableValue(v), false};
            }
            case ExprKind::Index:
            {
                const std::string read = "(*" + pointerCastOf(e) + elementAddress(e, scope) + ")";
                const Variable *root = rootVariable(e);
                if (scope.pendingCalls > 0 && e.type != Type::Record && root != nullptr &&
                    (reachable(*root, scope) || root->isParameter))
                {
                    return snapshot(storage(e), read, scope);
                }
                return {read, false};
            }
            case ExprKind::Member:
            {
                // A property (a call could set it), or a member of a record a
                // later call could change; a whole record is never copied here.
                const Variable *root = e.field != nullptr ? rootVariable(e) : nullptr;
                const bool changeable =
                    e.field == nullptr ||
                    (root != nullptr && (reachable(*root, scope) || root->isParameter));
                if (scope.pendingCalls > 0 && changeable && e.type != Type::Record && e.rank == 0)
                {
                    return snapshot(storage(e), member(e, scope).code, scope);
                }
                return member(e, scope);
            }
            case ExprKind::Call:
                return e.builtin != Builtin::None ? builtin(e, scope) : call(e, scope);
            case ExprKind::Unary:
            {
                const std::string operand = expr(*e.left, scope).code;
                if (e.text == "-")
                {
                    // Negating a Double cannot overflow; the integer types can.
                    return {e.type == Type::Double ? "(-" + operand + ")"
                                                   : opPrefix(e.type) + "neg(" + operand + ")",
                            false};
                }
                return {"(!" + operand + ")", false};
            }
            case ExprKind::Binary:
                return binary(e, scope);
        }
        return {"0", false};
    }

    // A Function call, evaluated into a temporary ahead of the statement.
    Value call(const Expr &e, StatementScope &scope)
    {
        // This call's own arguments are evaluated before it runs, so they
        // need no protection from it; only from calls after it.
        --scope.pendingCalls;
        const Procedure &callee = *e.procedure;
        std::vector<std::string> locks;
        const std::string code =
            "p_" + callee.name + "(" + arguments(callee, e.args, scope, locks) + ")";
        const std::string temp = scope.newTemp();
        if (locks.empty())
        {
            scope.prelude.push_back(declarationOf(*callee.result, temp) + " = " + code + ";");
        }
        else
        {
            scope.prelude.push_back(declarationOf(*callee.result, temp) + ";");
            scope.prelude.push_back(lockedAround(locks, temp + " = " + code + ";"));
        }
        if (callee.result->type == Type::String)
        {
            scope.releases.push_back("tf_str_release(" + temp + ");");
        }
        else if (callee.result->type == Type::Record)
        {
            scope.releases.push_back("t_" + callee.result->typeDef->name + "_free(&" + temp + ");");
        }
        else if (callee.result->type == Type::Ref)
        {
            scope.releases.push_back("tf_ref_release(" + temp + ");");
        }
        checkpoint(scope); // nothing after a failed call may run
        return {temp, false};
    }

    Value member(const Expr &e, StatementScope &scope)
    {
        if (e.left->isErr)
        {
            return {"tf_err_" + lowerCase(e.text) + "()", false}; // tf_err_number() ...
        }
        if (e.field != nullptr && e.left->type == Type::Ref) // through a Ref
        {
            return {deref(*e.left, expr(*e.left, scope).code) + ".f_" + e.text, false};
        }
        if (e.field != nullptr) // a record's member
        {
            return {parenthesised(expr(*e.left, scope).code) + ".f_" + e.text, false};
        }
        const ControlDef *control = e.left->control;
        const catalog::ControlType *type = catalog::findType(control->typeName);
        const catalog::Property *prop = catalog::findProperty(*type, e.text);
        const char *getter = prop->type == Type::String ? "tf_get_str" : "tf_get_int";
        return {std::string(getter) + "(c_" + control->name + ", " + prop->cName + ")", false};
    }

    Value binary(const Expr &e, StatementScope &scope)
    {
        const std::string &op = e.text;

        if (op == "Is" || op == "IsNot") // the same record, or both Nothing
        {
            const Value l = heldRef(*e.left, expr(*e.left, scope), scope);
            const Value r = heldRef(*e.right, expr(*e.right, scope), scope);
            return {"(" + l.code + (op == "Is" ? " == " : " != ") + r.code + ")", false};
        }

        if (op == "&" || (op == "+" && e.type == Type::String))
        {
            const Value l = borrowed(asString(*e.left, scope), scope);
            const Value r = borrowed(asString(*e.right, scope), scope);
            return {"tf_str_concat(" + l.code + ", " + r.code + ")", true};
        }

        const Type operandType = e.left->type;
        if (operandType == Type::String)
        {
            // String comparison.
            const Value l = borrowed(expr(*e.left, scope), scope);
            const Value r = borrowed(expr(*e.right, scope), scope);
            return {"(tf_str_compare(" + l.code + ", " + r.code + ") " + cOperator(op) + " 0)",
                    false};
        }

        // Arithmetic runs in the result's type; narrower operands widen through
        // the runtime function's prototype.
        const std::string l = expr(*e.left, scope).code;
        const std::string r = expr(*e.right, scope).code;
        if (op == "/" || op == "^")
        {
            return {std::string(op == "/" ? "tf_f64_div(" : "tf_f64_pow(") + l + ", " + r + ")",
                    false};
        }
        static const std::map<std::string, std::string> kArithmetic = {
            {"+", "add"}, {"-", "sub"}, {"*", "mul"}, {"\\", "div"}, {"Mod", "mod"}};
        if (auto it = kArithmetic.find(op); it != kArithmetic.end())
        {
            return {opPrefix(e.type) + it->second + "(" + l + ", " + r + ")", false};
        }
        // And/Or evaluate both sides (VB semantics); Booleans are 0 or 1.
        return {"(" + l + " " + cOperator(op) + " " + r + ")", false};
    }

    // The operand of &: Integers and Booleans become their text.
    Value asString(const Expr &e, StatementScope &scope)
    {
        const Type t = storage(e); // an Enum is text as its number
        if (t == Type::Integer)
        {
            return {"tf_str_from_i32(" + expr(e, scope).code + ")", true};
        }
        if (t == Type::Long)
        {
            return {"tf_str_from_i64(" + expr(e, scope).code + ")", true};
        }
        if (t == Type::Double)
        {
            return {"tf_str_from_f64(" + expr(e, scope).code + ")", true};
        }
        if (e.kind == ExprKind::BooleanLiteral ||
            (e.kind == ExprKind::Constant && e.type == Type::Boolean))
        {
            return {literal(e.integer != 0 ? "True" : "False"), false};
        }
        if (e.type == Type::Boolean)
        {
            // Separate statements: the order of operands of + is unspecified
            // in C++, and literal numbering must not depend on the compiler.
            const std::string condition = expr(e, scope).code;
            const std::string whenTrue = literal("True");
            const std::string whenFalse = literal("False");
            return {"(" + condition + " ? " + whenTrue + " : " + whenFalse + ")", false};
        }
        return expr(e, scope);
    }

    // Conversions and number functions (see tf_runtime.h for their rules).
    Value builtin(const Expr &e, StatementScope &scope)
    {
        if (e.builtin == Builtin::Count)
        {
            return {"tf_array_count(" + wholeArrayRef(*e.args[0], scope) + ")", false};
        }
        if (e.builtin == Builtin::LBound || e.builtin == Builtin::UBound)
        {
            const std::string dimension =
                e.args.size() > 1 ? settle(*e.args[1], expr(*e.args[1], scope), scope).code : "1";
            return {std::string(e.builtin == Builtin::LBound ? "tf_array_lbound("
                                                             : "tf_array_ubound(") +
                        wholeArrayRef(*e.args[0], scope) + ", " + dimension + ")",
                    false};
        }
        const Expr &arg = *e.args[0];
        const Type t = storage(arg); // CType to an Enum and CInt of one convert numbers
        if (e.builtin == Builtin::CStr)
        {
            return asString(arg, scope);
        }
        if (t == Type::String)
        {
            const std::string text = borrowed(expr(arg, scope), scope).code;
            switch (e.builtin)
            {
                case Builtin::CInt:
                    return {"tf_i32_from_str(" + text + ")", false};
                case Builtin::CLng:
                    return {"tf_i64_from_str(" + text + ")", false};
                case Builtin::CDbl:
                    return {"tf_f64_from_str(" + text + ")", false};
                default:
                    return {"tf_val(" + text + ")", false}; // Val
            }
        }
        const std::string x = expr(arg, scope).code;
        switch (e.builtin)
        {
            case Builtin::CInt:
                return {t == Type::Integer ? x
                        : t == Type::Long  ? "tf_i32_from_i64(" + x + ")"
                                           : "tf_i32_from_f64(" + x + ")",
                        false};
            case Builtin::CLng:
                return {t == Type::Integer ? "((int64_t)" + x + ")"
                        : t == Type::Long  ? x
                                           : "tf_i64_from_f64(" + x + ")",
                        false};
            case Builtin::CDbl:
                return {t == Type::Double ? x : "((double)" + x + ")", false};
            case Builtin::Str:
                return {(t == Type::Double ? "tf_str_str_f64(" : "tf_str_str_i64(") + x + ")",
                        true};
            case Builtin::Fix:
            case Builtin::Int:
                if (t != Type::Double)
                {
                    return {x, false}; // whole numbers already
                }
                return {(e.builtin == Builtin::Fix ? "tf_f64_fix(" : "tf_f64_int(") + x + ")",
                        false};
            default:
                return {x, false};
        }
    }

    // Parks an owned value in a temporary so it can be read and released.
    static Value borrowed(const Value &v, StatementScope &scope)
    {
        if (!v.owned)
        {
            return v;
        }
        const std::string name = scope.newTemp();
        scope.prelude.push_back("tf_string *" + name + " = " + v.code + ";");
        scope.releases.push_back("tf_str_release(" + name + ");");
        return {name, false};
    }

    static std::string cOperator(const std::string &op)
    {
        if (op == "=")
        {
            return "==";
        }
        if (op == "<>")
        {
            return "!=";
        }
        if (op == "And")
        {
            return "&";
        }
        if (op == "Or")
        {
            return "|";
        }
        return op; // < > <= >=
    }

    // -- Names, literals, layout -------------------------------------------

    std::string variableName(const Variable &v) const
    {
        if (v.isModuleLevel)
        {
            return "g_" + v.name;
        }
        if (v.isResult)
        {
            return "r_" + v.name;
        }
        return names_.at(&v);
    }

    // The variable as a C lvalue/rvalue: ByRef parameters are dereferenced.
    std::string variableValue(const Variable &v) const
    {
        return v.byRef ? "(*" + variableName(v) + ")" : variableName(v);
    }

    // The variable's address, for passing it ByRef or assigning a string.
    std::string variableAddress(const Variable &v) const
    {
        return v.byRef ? variableName(v) : "&" + variableName(v);
    }

    // The runtime's prefix for arithmetic in a numeric type: "tf_i64_" ...
    static std::string opPrefix(Type t)
    {
        return t == Type::Double ? "tf_f64_" : t == Type::Long ? "tf_i64_" : "tf_i32_";
    }

    // A whole number in C. Outside the Integer range it is INT64_C(...):
    // MSVC follows C89 and types a bare 3000000000 as a 32-bit unsigned
    // long, so -3000000000 would silently become 1294967296.
    static std::string integerLiteral(std::int64_t value)
    {
        const std::string digits = std::to_string(value);
        return value >= INT32_MIN && value <= INT32_MAX ? digits : "INT64_C(" + digits + ")";
    }

    // A C double literal: the shortest digits that read back exactly, with
    // a '.' or exponent so C does not take it for an integer.
    static std::string doubleLiteral(double value)
    {
        char buffer[40];
        for (int precision = 1; precision <= 17; ++precision)
        {
            std::snprintf(buffer, sizeof buffer, "%.*g", precision, value);
            if (std::strtod(buffer, nullptr) == value)
            {
                break;
            }
        }
        std::string text = buffer;
        if (text.find_first_of(".e") == std::string::npos)
        {
            text += ".0";
        }
        return text;
    }

    // -- Arrays --------------------------------------------------------------

    // The array as a tf_array *: parameters already are pointers.
    std::string arrayRef(const Variable &v) const
    {
        return (v.isParameter ? "" : "&") + variableName(v);
    }

    static const char *elemKind(const Variable &v)
    {
        switch (storage(v))
        {
            case Type::Long:
                return "TF_ELEM_I64";
            case Type::Double:
                return "TF_ELEM_F64";
            case Type::Boolean:
                return "TF_ELEM_BOOL";
            case Type::String:
                return "TF_ELEM_STRING";
            case Type::Record:
                return "TF_ELEM_RECORD";
            case Type::Ref:
                return "TF_ELEM_REF";
            default:
                return "TF_ELEM_I32";
        }
    }

    // "(int32_t *)", "(tf_string **)", "(t_Customer *)": an address as its type.
    static std::string pointerCastOf(const Expr &e)
    {
        const std::string type = cTypeOf(e);
        return "(" + type + (type.back() == '*' ? "*" : " *") + ")";
    }

    // The C type of a variable's or an expression's value.
    static std::string cTypeOf(const Variable &v) { return cTypeOf(v.type, v.typeDef, storage(v)); }

    static std::string cTypeOf(const Expr &e) { return cTypeOf(e.type, e.typeDef, storage(e)); }

    // A record is t_<Type>; a Ref a pointer to one (Nothing: void *).
    static std::string cTypeOf(Type type, const TypeDef *def, Type stored)
    {
        if (type == Type::Record)
        {
            return "t_" + def->name;
        }
        if (type == Type::Ref)
        {
            return def != nullptr ? "t_" + def->name + " *" : "void *";
        }
        return cType(stored);
    }

    static std::string declarationOf(const Variable &v, const std::string &name)
    {
        const std::string type = cTypeOf(v);
        return type + (type.back() == '*' ? "" : " ") + name;
    }

    static std::string declarationOf(const Expr &e, const std::string &name)
    {
        const std::string type = cTypeOf(e);
        return type + (type.back() == '*' ? "" : " ") + name;
    }

    // The tf_record_ops of a record array's element Type, else NULL.
    std::string elemOps(const Variable &v)
    {
        return v.type == Type::Record ? opsOf(*v.typeDef) : "NULL";
    }

    // "&t_Customer_ops", noting that the Type's ops must be emitted.
    std::string opsOf(const TypeDef &def)
    {
        opsNeeded_.insert(&def);
        return "&t_" + def.name + "_ops";
    }

    // "(const int32_t[]){10, 20}": a shape for the runtime.
    static std::string shapeList(const std::vector<std::string> &bounds)
    {
        std::string out = "(const int32_t[]){";
        for (std::size_t i = 0; i < bounds.size(); ++i)
        {
            out += (i == 0 ? "" : ", ") + bounds[i];
        }
        return out + "}";
    }

    std::string arrayInit(const Variable &v, const std::string &ref)
    {
        std::vector<std::string> bounds;
        for (std::int32_t u : v.upper)
        {
            bounds.push_back(std::to_string(u));
        }
        return "tf_array_init(" + ref + ", " + elemKind(v) + ", " + elemOps(v) + ", " +
               std::to_string(v.rank) + ", " + shapeList(bounds) + ");";
    }

    // The address of an element, its indexes checked: tf_array_at1(&v_a, i).
    std::string elementAddress(const Expr &e, StatementScope &scope)
    {
        return elementAt(e, elementArray(e, scope), scope);
    }

    // tf_array_at1(ref, i) and its kin, the indexes evaluated left to right.
    std::string elementAt(const Expr &e, const std::string &ref, StatementScope &scope)
    {
        std::vector<std::string> indexes;
        for (const auto &index : e.args)
        {
            indexes.push_back(settle(*index, expr(*index, scope), scope).code);
        }
        if (e.linear) // For Each's element n
        {
            return "tf_array_atlin(" + ref + ", " + indexes[0] + ")";
        }
        if (indexes.size() == 1)
        {
            return "tf_array_at1(" + ref + ", " + indexes[0] + ")";
        }
        if (indexes.size() == 2)
        {
            return "tf_array_at2(" + ref + ", " + indexes[0] + ", " + indexes[1] + ")";
        }
        std::string list = "(const int64_t[]){";
        for (std::size_t i = 0; i < indexes.size(); ++i)
        {
            list += (i == 0 ? "" : ", ") + indexes[i];
        }
        return "tf_array_atn(" + ref + ", " + std::to_string(indexes.size()) + ", " + list + "})";
    }

    // ReDim [Preserve] a(n, m), ...
    std::string reDim(const Stmt &s, StatementScope &scope)
    {
        std::string out;
        for (const auto &target : s.args)
        {
            std::vector<std::string> bounds;
            for (const auto &bound : target->args)
            {
                bounds.push_back(settle(*bound, expr(*bound, scope), scope).code);
            }
            // a(n) is the variable; o.Lines(n) the member array.
            const Variable &v = target->left != nullptr ? *target->left->field : *target->variable;
            const std::string ref =
                target->left != nullptr ? wholeArrayRef(*target->left, scope) : arrayRef(v);
            out += (out.empty() ? "" : " ") + std::string("tf_array_redim(") + ref + ", " +
                   elemKind(v) + ", " + elemOps(v) + ", " + (s.preserve ? "1" : "0") + ", " +
                   std::to_string(v.rank) + ", " + shapeList(bounds) + ");";
        }
        return out;
    }

    // "lock(a); code unlock(a);" around a call that receives elements ByRef.
    static std::string lockedAround(const std::vector<std::string> &locks, const std::string &code)
    {
        std::string out;
        for (const std::string &ref : locks)
        {
            out += "tf_array_lock(" + ref + "); ";
        }
        out += code;
        for (const std::string &ref : locks)
        {
            out += " tf_array_unlock(" + ref + ");";
        }
        return out;
    }

    // The type a value is stored as in C: an Enum as its underlying type.
    static Type storage(const Variable &v)
    {
        return v.type == Type::Enum && v.enumDef != nullptr ? v.enumDef->underlying : v.type;
    }

    static Type storage(const Expr &e)
    {
        return e.type == Type::Enum && e.enumDef != nullptr ? e.enumDef->underlying : e.type;
    }

    // A Const or Enum member's value as C.
    std::string constantCode(const Expr &e)
    {
        switch (e.type)
        {
            case Type::String:
                return literal(e.text);
            case Type::Double:
                return doubleLiteral(e.real);
            case Type::Boolean:
                return e.integer != 0 ? "1" : "0";
            default:
                return integerLiteral(e.integer);
        }
    }

    static std::string cType(Type t)
    {
        switch (t)
        {
            case Type::String:
                return "tf_string *";
            case Type::Boolean:
                return "int";
            case Type::Long:
                return "int64_t";
            case Type::Double:
                return "double";
            default:
                return "int32_t";
        }
    }

    // "int32_t v_count", "tf_string *v_name", "int v_done" (Boolean).
    static std::string declaration(Type t, const std::string &name)
    {
        const std::string type = cType(t);
        return type + (type.back() == '*' ? "" : " ") + name;
    }

    static const char *zero(Type t) { return t == Type::String || t == Type::Ref ? "NULL" : "0"; }

    // A borrowed reference to a literal, created on first use. The empty
    // string is NULL, so "" needs no literal at all.
    std::string literal(const std::string &text)
    {
        if (text.empty())
        {
            return "NULL";
        }
        auto [it, inserted] = literalIndex_.emplace(text, literals_.size());
        if (inserted)
        {
            literals_.push_back(text);
        }
        return "tf_str_literal(&s_" + std::to_string(it->second) + ", " + cString(text) + ", " +
               std::to_string(text.size()) + ")";
    }

    // A C string literal. Anything outside printable ASCII is written as a
    // three-digit octal escape, which can never merge with following digits.
    static std::string cString(const std::string &text)
    {
        static const char kOctal[] = "01234567";
        std::string out = "\"";
        for (unsigned char c : text)
        {
            if (c == '"' || c == '\\' || c == '?')
            {
                out += '\\';
                out += static_cast<char>(c);
            }
            else if (c < 0x20 || c >= 0x7F)
            {
                out += '\\';
                out += kOctal[(c >> 6) & 7];
                out += kOctal[(c >> 3) & 7];
                out += kOctal[c & 7];
            }
            else
            {
                out += static_cast<char>(c);
            }
        }
        out += '"';
        return out;
    }

    void line(std::ostringstream &out, Location where) const
    {
        out << "#line " << where.line << ' ' << cString(options_.sourcePath) << '\n';
    }

    static std::string indent(int depth)
    {
        return std::string(static_cast<std::size_t>(depth) * 4, ' ');
    }

    const Module &module_;
    const EmitOptions &options_;
    std::vector<std::string> literals_;
    std::map<std::string, std::size_t> literalIndex_;

    // State for the procedure being emitted.
    const Procedure *current_ = nullptr;
    std::map<const Variable *, std::string> names_; // parameters and locals
    std::string cleanup_;                           // releases at End Sub
    bool usesCleanupLabel_ = false;
    std::vector<const Stmt *> loopStack_; // innermost last
    std::map<const Stmt *, int> loopIds_; // numbering for exit_<n>, to_<n>
    std::set<const Stmt *> loopLabels_;   // loops an Exit jumps past
    int loopCount_ = 0;
    std::vector<TryContext> tries_; // Trys around the statement, innermost last
    int tryCount_ = 0;
    bool checks_ = false;                 // the program has a Try, so errors are tested for
    std::set<const TypeDef *> opsNeeded_; // Types whose tf_record_ops some code uses
};

} // namespace

std::string emitC(const ast::Module &module, const EmitOptions &options)
{
    return Emitter(module, options).run();
}

} // namespace tf
