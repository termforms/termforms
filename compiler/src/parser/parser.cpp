// SPDX-License-Identifier: BSD-2-Clause
//
// parser.cpp -- see parser.hpp.
//
// Grammar of this version (keywords upper case, EOS = end of statement):
//
//   module     := { formBlock | sub | function | enum | typeBlock | dim EOS | const EOS }
//   typeBlock  := TYPE name EOS { name [shape] AS type EOS } END TYPE
//   formBlock  := BEGIN Form name EOS { setting | control } END EOS
//   control    := BEGIN type name EOS { setting } END EOS
//   setting    := name '=' ['-'] literal EOS
//   sub        := SUB name [params] EOS block END SUB EOS
//   function   := FUNCTION name [params] [AS type] EOS block END FUNCTION EOS
//   params     := '(' [param { ',' param }] ')'
//   param      := [BYVAL | BYREF] name ['(' { ',' } ')'] [AS type]
//   statement  := dim | if | for | do | while | try | with | EXIT what | RETURN [expr]
//               | THROW [expr [',' expr]] | Err.Raise args | Err.Clear
//               | CALL name ['(' [args] ')'] | END
//               | target '=' expr | name [args] | name '(' [args] ')'
//   dim        := DIM name [shape] [AS type] { ',' name [shape] [AS type] }
//   shape      := '(' expr { ',' expr } ')'   a fixed array: upper bounds
//               | '(' { ',' } ')'             a dynamic array: () or (,) ...
//   redim      := REDIM [PRESERVE] name '(' expr { ',' expr } ')' { ',' ... }
//   erase      := ERASE name { ',' name }
//   const      := CONST name [AS type] '=' expr { ',' name [AS type] '=' expr }
//   enum       := ENUM name [AS type] EOS { name ['=' expr] EOS } END ENUM
//   type       := INTEGER | LONG | DOUBLE | STRING | BOOLEAN | name (an Enum)
//   if         := IF expr THEN EOS block { ELSEIF expr THEN EOS block }
//                 [ELSE EOS block] END IF
//               | IF expr THEN statement [ELSE statement]   (single line)
//   for        := FOR name '=' expr TO expr [STEP expr] EOS block NEXT [name]
//               | FOR EACH name IN expr EOS block NEXT [name]
//   do         := DO [(WHILE | UNTIL) expr] EOS block LOOP [(WHILE | UNTIL) expr]
//   while      := WHILE expr EOS block (WEND | END WHILE)
//   try        := TRY EOS block { CATCH [WHEN expr] EOS block }
//                 [FINALLY EOS block] END TRY
//   with       := WITH expr EOS block END WITH   (inside: .name is a member of expr)
//   args       := expr { ',' expr }
//   expr       := or-expression, with VB precedence from lowest to highest:
//                 Or, And, Not, comparisons, &, + -, Mod, \, * /, unary -, ^
//   primary    := (literal | name ['(' [args] ')'] | '(' expr ')') { '.' name ['(' [args] ')'] }
//               | CTYPE '(' expr ',' type ')'
//
// A block ends at a keyword that closes a construct (Next, Loop, Wend,
// Else, ElseIf, Catch, Finally, End If/Sub/Function/While/Try) if some
// construct still open
// around it can take that closer; the construct then checks that the closer
// is its own. A closer nothing open can take ("Loop" without "Do") is
// reported and skipped. Either way one misplaced closer produces one error
// instead of derailing the rest of the file.

#include "parser/parser.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace tf
{

namespace
{

using namespace tf::ast;

class Parser
{
public:
    Parser(const std::vector<Token> &tokens, Diagnostics &diagnostics)
        : tokens_(tokens), diagnostics_(diagnostics)
    {
    }

    Module run()
    {
        Module module;
        module.fileName =
            diagnostics_.fileName(tokens_.empty() ? 0 : tokens_.front().location.file);
        skipStatementEnds();

        while (!at(TokenKind::EndOfFile))
        {
            // Public / Private before a module-level declaration (VB's
            // defaults otherwise: Subs, Functions, Types, Enums Public;
            // variables and Consts Private).
            if (peek().isKeyword("PUBLIC") || peek().isKeyword("PRIVATE"))
            {
                parseVisible(module);
            }
            else if (peek().isKeyword("BEGIN"))
            {
                parseFormBlock(module);
            }
            else if (peek().isKeyword("SUB") || peek().isKeyword("FUNCTION"))
            {
                module.procedures.push_back(parseProcedure());
            }
            else if (peek().isKeyword("DIM"))
            {
                module.moduleStatements.push_back(parseDim(&module, nullptr));
                expectEndOfStatement();
            }
            else if (peek().isKeyword("CONST"))
            {
                module.moduleStatements.push_back(parseConst(&module, nullptr));
                expectEndOfStatement();
            }
            else if (peek().isKeyword("ENUM"))
            {
                parseEnum(module);
            }
            else if (peek().isKeyword("TYPE"))
            {
                parseType(module);
            }
            else
            {
                error(peek().location, "statements must be inside a Sub or Function");
                skipStatement();
            }
            skipStatementEnds();
        }
        gatherInitialisers(module);
        return module;
    }

    // Module-level initialisers, in order, become a hidden Sub that the
    // program runs at start (its name cannot be written in BASIC).
    static void gatherInitialisers(Module &module)
    {
        Block init;
        for (auto &stmt : module.moduleStatements)
        {
            if (stmt != nullptr && stmt->kind == StmtKind::Dim)
            {
                for (auto &s : stmt->body)
                {
                    init.push_back(std::move(s));
                }
                stmt->body.clear();
            }
        }
        if (init.empty())
        {
            return;
        }
        auto proc = std::make_unique<Procedure>();
        proc->name = "_init";
        proc->isPrivate = true;
        proc->location = init.front()->location;
        proc->endLocation = init.back()->location;
        proc->body = std::move(init);
        module.initialiser = proc.get();
        // First, so it is checked (and its messages come) before the Subs,
        // as module-level Dims usually come before them.
        module.procedures.insert(module.procedures.begin(), std::move(proc));
    }

private:
    // -- Token access ------------------------------------------------------

    const Token &peek(std::size_t ahead = 0) const
    {
        const std::size_t i = pos_ + ahead;
        return i < tokens_.size() ? tokens_[i] : tokens_.back();
    }
    bool at(TokenKind kind) const { return peek().kind == kind; }
    const Token &next() { return tokens_[pos_ < tokens_.size() - 1 ? pos_++ : pos_]; }

    bool acceptKeyword(std::string_view keyword)
    {
        if (peek().isKeyword(keyword))
        {
            next();
            return true;
        }
        return false;
    }
    bool acceptSymbol(std::string_view symbol)
    {
        if (peek().isSymbol(symbol))
        {
            next();
            return true;
        }
        return false;
    }

    void error(Location where, const std::string &message)
    {
        // One error per statement: later ones are almost always fallout.
        if (where.line != lastErrorLine_)
        {
            diagnostics_.error(where, message);
            lastErrorLine_ = where.line;
        }
    }

    static std::string describe(const Token &t)
    {
        switch (t.kind)
        {
            case TokenKind::EndOfFile:
                return "end of file";
            case TokenKind::EndOfStatement:
                return "end of line";
            case TokenKind::Keyword:
                return "'" + t.spelling + "'";
            case TokenKind::StringLiteral:
                return "a string";
            case TokenKind::IntegerLiteral:
                return "'" + t.text + "'";
            default:
                return "'" + t.text + "'";
        }
    }

    bool expectKeyword(std::string_view keyword, std::string_view display)
    {
        if (acceptKeyword(keyword))
        {
            return true;
        }
        error(peek().location,
              "expected '" + std::string(display) + "' but found " + describe(peek()));
        return false;
    }

    // "End <keyword>". Another End (End Sub where End With was due) is left
    // for whatever it closes, so a missing closer is reported once.
    bool expectEndOf(std::string_view keyword, std::string_view display)
    {
        if (isEndOf(keyword))
        {
            next();
            next();
            return true;
        }
        error(peek().location, "expected '" + std::string(display) + "' but found " +
                                   (peek().isKeyword("END") && peek(1).kind == TokenKind::Keyword
                                        ? "'" + closerText() + "'"
                                        : describe(peek())));
        return false;
    }

    bool expectSymbol(std::string_view symbol)
    {
        if (acceptSymbol(symbol))
        {
            return true;
        }
        error(peek().location,
              "expected '" + std::string(symbol) + "' but found " + describe(peek()));
        return false;
    }

    // A member's name, after a '.' or in a Type block: a keyword is allowed
    // there (Next As Ref Node, p.Next), as VB.NET allows rnd.Next(), since
    // nothing else could stand in that place. It keeps the case written.
    std::string expectMemberName()
    {
        if (at(TokenKind::Keyword))
        {
            return next().spelling;
        }
        return expectIdentifier("a member or property name");
    }

    std::string expectIdentifier(const char *what)
    {
        if (at(TokenKind::Identifier))
        {
            return next().text;
        }
        error(peek().location, std::string("expected ") + what + " but found " + describe(peek()));
        return {};
    }

    void expectEndOfStatement()
    {
        if (at(TokenKind::EndOfStatement) || at(TokenKind::EndOfFile))
        {
            if (at(TokenKind::EndOfStatement))
            {
                next();
            }
            return;
        }
        error(peek().location, "expected end of statement but found " + describe(peek()));
        skipStatement();
    }

    void skipStatement()
    {
        while (!at(TokenKind::EndOfStatement) && !at(TokenKind::EndOfFile))
        {
            next();
        }
        if (at(TokenKind::EndOfStatement))
        {
            next();
        }
    }

    void skipStatementEnds()
    {
        while (at(TokenKind::EndOfStatement))
        {
            next();
        }
    }

    // -- Forms -------------------------------------------------------------

    void parseFormBlock(Module &module)
    {
        const Location where = next().location; // BEGIN
        auto form = std::make_unique<ControlDef>();
        form->location = where;
        form->isForm = true;
        form->typeName = expectIdentifier("'Form'");
        if (!form->typeName.empty() && form->typeName != "Form")
        {
            error(where,
                  "the outer block must be 'Begin Form', not 'Begin " + form->typeName + "'");
        }
        form->name = expectIdentifier("a form name");
        expectEndOfStatement();
        skipStatementEnds();

        while (!at(TokenKind::EndOfFile) && !peek().isKeyword("END"))
        {
            if (peek().isKeyword("BEGIN"))
            {
                form->children.push_back(parseControlBlock());
            }
            else
            {
                parseSetting(*form);
            }
            skipStatementEnds();
        }
        expectKeyword("END", "End");
        expectEndOfStatement();

        if (module.form != nullptr)
        {
            error(where, "only one form per file is supported in this version");
            return;
        }
        module.form = std::move(form);
    }

    std::unique_ptr<ControlDef> parseControlBlock()
    {
        auto control = std::make_unique<ControlDef>();
        control->location = next().location; // BEGIN
        control->typeName = expectIdentifier("a control type");
        control->name = expectIdentifier("a control name");
        expectEndOfStatement();
        skipStatementEnds();

        while (!at(TokenKind::EndOfFile) && !peek().isKeyword("END"))
        {
            if (peek().isKeyword("BEGIN"))
            {
                control->children.push_back(parseControlBlock()); // a Frame's (sema checks)
            }
            else
            {
                parseSetting(*control);
            }
            skipStatementEnds();
        }
        expectKeyword("END", "End");
        expectEndOfStatement();
        return control;
    }

    void parseSetting(ControlDef &owner)
    {
        PropertySetting setting;
        setting.location = peek().location;
        setting.name = expectIdentifier("a property name");
        if (setting.name.empty())
        {
            skipStatement();
            return;
        }
        if (!expectSymbol("="))
        {
            skipStatement();
            return;
        }

        // Only literal values: the form block is data, not code.
        const bool negative = acceptSymbol("-");
        auto value = std::make_unique<Expr>();
        value->location = peek().location;
        if (at(TokenKind::IntegerLiteral))
        {
            value->kind = ExprKind::IntegerLiteral;
            value->integer = negative ? -next().integer : next().integer;
        }
        else if (at(TokenKind::DoubleLiteral))
        {
            value->kind = ExprKind::DoubleLiteral;
            value->real = negative ? -next().real : next().real;
        }
        else if (!negative && at(TokenKind::StringLiteral))
        {
            value->kind = ExprKind::StringLiteral;
            value->text = next().text;
        }
        else if (!negative && (peek().isKeyword("TRUE") || peek().isKeyword("FALSE")))
        {
            value->kind = ExprKind::BooleanLiteral;
            value->integer = next().isKeyword("TRUE") ? 1 : 0;
        }
        else
        {
            error(peek().location, "a property value must be a number, a string, True or False");
            skipStatement();
            return;
        }
        setting.value = std::move(value);
        owner.properties.push_back(std::move(setting));
        expectEndOfStatement();
    }

    // -- Procedures --------------------------------------------------------

    std::unique_ptr<Procedure> parseProcedure()
    {
        auto proc = std::make_unique<Procedure>();
        proc->isFunction = peek().isKeyword("FUNCTION");
        const char *kind = proc->isFunction ? "Function" : "Sub";
        proc->location = next().location; // SUB or FUNCTION

        const char suffix = peek().suffix;
        const Location nameAt = peek().location;
        proc->name = expectIdentifier(proc->isFunction ? "a Function name" : "a Sub name");
        if (suffix != 0 && !proc->isFunction)
        {
            error(nameAt, "a Sub returns no value, so its name cannot have a type suffix");
        }
        if (acceptSymbol("("))
        {
            parseParameters(*proc);
            expectSymbol(")");
        }

        if (proc->isFunction)
        {
            auto result = std::make_unique<Variable>();
            result->name = proc->name;
            result->location = nameAt;
            result->isResult = true;
            result->type = typeFromSuffix(suffix);
            parseOptionalAs(*result, "Function '" + proc->name + "'");
            proc->result = std::move(result);
        }
        else if (peek().isKeyword("AS"))
        {
            error(peek().location, "a Sub returns no value; use a Function to return one");
            while (!at(TokenKind::EndOfStatement) && !at(TokenKind::EndOfFile))
            {
                next();
            }
        }
        expectEndOfStatement();

        currentProcedure_ = proc.get();
        proc->body = parseBlock();
        currentProcedure_ = nullptr;

        proc->endLocation = peek().location;
        const char *closer = proc->isFunction ? "FUNCTION" : "SUB";
        if (isEndOf(proc->isFunction ? "SUB" : "FUNCTION"))
        {
            error(peek().location, std::string("a ") + kind + " ends with 'End " + kind + "'");
            next();
            next();
        }
        else if (expectKeyword("END", proc->isFunction ? "End Function" : "End Sub") &&
                 expectKeyword(closer, kind))
        {
            expectEndOfStatement();
        }
        return proc;
    }

    void parseParameters(Procedure &proc)
    {
        if (peek().isSymbol(")"))
        {
            return;
        }
        for (;;)
        {
            auto p = std::make_unique<Variable>();
            p->isParameter = true;
            if (acceptKeyword("BYREF"))
            {
                p->byRef = true;
            }
            else
            {
                acceptKeyword("BYVAL"); // the default, accepted for clarity
            }
            p->location = peek().location;
            const char suffix = peek().suffix;
            p->name = expectIdentifier("a parameter name");
            if (p->name.empty())
            {
                while (!at(TokenKind::EndOfStatement) && !peek().isSymbol(")"))
                {
                    next();
                }
                return;
            }
            p->type = typeFromSuffix(suffix);
            if (peek().isSymbol("("))
            {
                parseShape(*p, false);
            }
            parseOptionalAs(*p, "parameter '" + p->name + "'");
            proc.parameters.push_back(std::move(p));
            if (!acceptSymbol(","))
            {
                return;
            }
        }
    }

    // An array's "(...)": "(10, 20)" gives a fixed array its upper bounds
    // (only in a Dim); "()" or "(,)" a dynamic array or array parameter of
    // that many dimensions.
    void parseShape(Variable &v, bool boundsAllowed)
    {
        next(); // '('
        if (peek().isSymbol(")") || peek().isSymbol(","))
        {
            v.rank = 1;
            while (acceptSymbol(","))
            {
                ++v.rank;
            }
            v.isDynamic = true;
            expectSymbol(")");
            return;
        }
        if (!boundsAllowed)
        {
            error(peek().location,
                  "an array parameter has no size: write " + v.name + "() or " + v.name + "(,)");
        }
        do
        {
            ExprPtr bound = parseExpression();
            if (peek().isKeyword("TO"))
            {
                // Arrays start at 0 (decided 27 Oct 2026): no lower bounds.
                error(peek().location, "arrays start at 0: write " + v.name +
                                           "(n) for elements 0 to n, not a range with 'To'");
                next();
                bound = parseExpression();
            }
            v.bounds.push_back(std::move(bound));
        } while (acceptSymbol(","));
        v.rank = static_cast<int>(v.bounds.size());
        expectSymbol(")");
    }

    // "[As type]" after a parameter or Function name, which has no Variant
    // to fall back on: the type comes from the suffix or from As.
    void parseOptionalAs(Variable &v, const std::string &what)
    {
        if (acceptKeyword("AS"))
        {
            const Location typeAt = peek().location;
            const Type declared = parseTypeName(&v.typeName);
            takeDictionary(v, typeAt);
            if (v.type != Type::Error && declared != Type::Error && v.type != declared)
            {
                error(typeAt, "'" + v.name + "' has a type suffix that contradicts 'As " +
                                  typeName(declared) + "'");
            }
            v.type = declared;
            return;
        }
        if (v.type == Type::Error)
        {
            error(v.location, what + " needs a type, such as 'As Integer' or 'As String'");
        }
    }

    // True at "End <keyword>".
    bool isEndOf(std::string_view keyword) const
    {
        return peek().isKeyword("END") && peek(1).isKeyword(keyword);
    }

    // True at a keyword that closes some enclosing construct.
    bool atBlockCloser() const
    {
        const Token &t = peek();
        return t.isKeyword("NEXT") || t.isKeyword("LOOP") || t.isKeyword("WEND") ||
               t.isKeyword("ELSE") || t.isKeyword("ELSEIF") || t.isKeyword("SUB") ||
               t.isKeyword("FUNCTION") || t.isKeyword("CATCH") || t.isKeyword("FINALLY") ||
               isEndOf("IF") || isEndOf("SUB") || isEndOf("FUNCTION") || isEndOf("WHILE") ||
               isEndOf("TRY") || isEndOf("WITH") || t.isKeyword("CASE") || isEndOf("SELECT");
    }

    // The body of a construct opened with `opener` ("FOR", "DO", "WHILE",
    // "IF", "TRY", "WITH", "SELECT"; empty for a procedure body).
    Block parseBlock(const char *opener = "")
    {
        open_.push_back(opener);
        Block block;
        skipStatementEnds();
        while (!at(TokenKind::EndOfFile))
        {
            if (atBlockCloser())
            {
                const char *wants = openerFor();
                if (wants == nullptr || isOpen(wants))
                {
                    break;
                }
                error(peek().location,
                      "'" + closerText() + "' without '" + displayName(wants) + "'");
                skipStatement();
                skipStatementEnds();
                continue;
            }
            if (StmtPtr s = parseStatement())
            {
                block.push_back(std::move(s));
            }
            skipStatementEnds();
        }
        open_.pop_back();
        return block;
    }

    // The construct the closer at the current token belongs to, or null for
    // the end (or start) of a procedure, which always ends the block.
    const char *openerFor() const
    {
        const Token &t = peek();
        if (t.isKeyword("NEXT"))
        {
            return "FOR";
        }
        if (t.isKeyword("LOOP"))
        {
            return "DO";
        }
        if (t.isKeyword("WEND") || isEndOf("WHILE"))
        {
            return "WHILE";
        }
        if (t.isKeyword("ELSE") || t.isKeyword("ELSEIF") || isEndOf("IF"))
        {
            return "IF";
        }
        if (t.isKeyword("CATCH") || t.isKeyword("FINALLY") || isEndOf("TRY"))
        {
            return "TRY";
        }
        if (isEndOf("WITH"))
        {
            return "WITH";
        }
        if (t.isKeyword("CASE") || isEndOf("SELECT"))
        {
            return "SELECT";
        }
        return nullptr;
    }

    // "WHILE" -> "While", for messages.
    static std::string displayName(std::string_view keyword)
    {
        std::string name(keyword);
        for (std::size_t i = 1; i < name.size(); ++i)
        {
            name[i] = static_cast<char>(name[i] - 'A' + 'a');
        }
        return name;
    }

    bool isOpen(std::string_view opener) const
    {
        for (const char *o : open_)
        {
            if (opener == o)
            {
                return true;
            }
        }
        return false;
    }

    std::string closerText() const
    {
        return peek().isKeyword("END") ? "End " + peek(1).spelling : peek().spelling;
    }

    // -- Statements --------------------------------------------------------

    StmtPtr parseStatement()
    {
        const Token &t = peek();

        if (t.isKeyword("DIM"))
        {
            StmtPtr s = parseDim(nullptr, currentProcedure_);
            expectEndOfStatement();
            return s;
        }
        if (t.isKeyword("IF"))
        {
            return parseIf();
        }
        if (t.isKeyword("FOR"))
        {
            return parseFor();
        }
        if (t.isKeyword("DO"))
        {
            return parseDo();
        }
        if (t.isKeyword("WHILE"))
        {
            return parseWhile();
        }
        if (t.isKeyword("TRY"))
        {
            return parseTry();
        }
        if (t.isKeyword("WITH"))
        {
            return parseWith();
        }
        if (t.isKeyword("SELECT"))
        {
            return parseSelect();
        }
        if (t.isKeyword("CONST"))
        {
            StmtPtr s = parseConst(nullptr, currentProcedure_);
            expectEndOfStatement();
            return s;
        }
        if (t.isKeyword("ENUM"))
        {
            error(t.location, "an Enum is declared at module level, outside any Sub or Function");
            while (!at(TokenKind::EndOfFile) && !isEndOf("ENUM") && !isEndOf("SUB") &&
                   !isEndOf("FUNCTION"))
            {
                next();
            }
            if (isEndOf("ENUM"))
            {
                next();
                next();
            }
            return nullptr;
        }
        StmtPtr s = parseSimpleStatement();
        if (s == nullptr)
        {
            skipStatement();
            return nullptr;
        }
        expectEndOfStatement();
        return s;
    }

    // A statement that fits on one line, so it may also follow Then or Else
    // in a single-line If. Returns null after reporting an error.
    StmtPtr parseSimpleStatement()
    {
        const Token &t = peek();

        if (t.isKeyword("END") && atStatementEnd(1))
        {
            auto s = std::make_unique<Stmt>();
            s->kind = StmtKind::End;
            s->location = next().location;
            return s;
        }
        if (t.isKeyword("CALL"))
        {
            next();
            auto s = std::make_unique<Stmt>();
            s->kind = StmtKind::Call;
            s->location = peek().location;
            s->callee = expectIdentifier("a Sub name");
            if (acceptSymbol("("))
            {
                parseArguments(s->args);
                expectSymbol(")");
            }
            return s;
        }
        if (t.isKeyword("EXIT"))
        {
            return parseExit();
        }
        if (t.isKeyword("RETURN"))
        {
            auto s = std::make_unique<Stmt>();
            s->kind = StmtKind::Return;
            s->location = next().location;
            if (!atStatementEnd())
            {
                s->value = parseExpression();
            }
            return s;
        }
        if (t.isKeyword("REDIM"))
        {
            auto s = std::make_unique<Stmt>();
            s->kind = StmtKind::ReDim;
            s->location = next().location;
            s->preserve = acceptKeyword("PRESERVE");
            do
            {
                // a(n) or o.Lines(n): an array and its new bounds.
                if (!at(TokenKind::Identifier) && !peek().isSymbol("."))
                {
                    error(peek().location, "expected an array name but found " + describe(peek()));
                    return nullptr;
                }
                s->args.push_back(parsePrimary());
            } while (acceptSymbol(","));
            return s;
        }
        if (t.isKeyword("ERASE"))
        {
            auto s = std::make_unique<Stmt>();
            s->kind = StmtKind::Erase;
            s->location = next().location;
            do
            {
                if (!at(TokenKind::Identifier) && !peek().isSymbol("."))
                {
                    error(peek().location, "expected an array name but found " + describe(peek()));
                    return nullptr;
                }
                s->args.push_back(parsePrimary()); // a, o.Lines or .Lines
            } while (acceptSymbol(","));
            return s;
        }
        if (StmtPtr s = parseFileStatement(); s != nullptr || failed_)
        {
            failed_ = false;
            return s;
        }
        if (t.isKeyword("THROW"))
        {
            auto s = std::make_unique<Stmt>();
            s->kind = StmtKind::Throw;
            s->location = next().location;
            if (!atStatementEnd())
            {
                parseArguments(s->args);
            }
            return s;
        }
        // Err.Raise and Err.Clear are statements; other Err members are read.
        if (t.kind == TokenKind::Identifier && t.text == "Err" && peek(1).isSymbol(".") &&
            (peek(2).text == "Raise" || peek(2).text == "Clear"))
        {
            auto s = std::make_unique<Stmt>();
            s->location = t.location;
            next();
            next();
            if (next().text == "Clear")
            {
                s->kind = StmtKind::ErrClear;
                return s;
            }
            s->kind = StmtKind::Throw;
            s->viaErrRaise = true;
            if (!atStatementEnd())
            {
                parseArguments(s->args);
            }
            return s;
        }
        if (t.kind == TokenKind::Identifier)
        {
            // "name = ..." or "name.Prop = ..." is an assignment; anything
            // else starting with a name is a call ("Bump .Age" in a With).
            if (peek(1).isSymbol("=") || (peek(1).isSymbol(".") && !peek(1).spaced) ||
                (peek(1).isSymbol("(") && (parenthesisedThen(1, "=") || parenthesisedThen(1, "."))))
            {
                return parseAssignOrMethod();
            }
            return parseCallStatement();
        }
        if (t.isSymbol(".") || t.isKeyword("ME")) // .Name = ... in a With; Me.Caption = ...
        {
            return parseAssignOrMethod();
        }
        if (t.isKeyword("DIM") || t.isKeyword("IF") || t.isKeyword("FOR") || t.isKeyword("DO") ||
            t.isKeyword("WHILE") || t.isKeyword("TRY") || t.isKeyword("WITH") ||
            t.isKeyword("SELECT"))
        {
            error(t.location, "'" + t.spelling + "' cannot follow 'Then' or 'Else' on one line");
            return nullptr;
        }
        error(t.location, "expected a statement but found " + describe(t));
        return nullptr;
    }

    // -- Files ---------------------------------------------------------------

    // VB's file words are not reserved (programs use Append, Line, Input as
    // names): a statement is a file statement by its shape. Print, Close
    // before #; Close alone or before a number; Line Input #; Open when the
    // statement has For. Null if this is not one (failed_ set on an error).
    StmtPtr parseFileStatement()
    {
        const Token &t = peek();
        if (isWord(t, "OPEN") && statementHas("FOR"))
        {
            return parseOpen();
        }
        if (isWord(t, "CLOSE") && (peek(1).isSymbol("#") || atStatementEnd(1) ||
                                   peek(1).kind == TokenKind::IntegerLiteral))
        {
            return parseClose();
        }
        if (isWord(t, "PRINT") && peek(1).isSymbol("#"))
        {
            return parsePrint();
        }
        if (isWord(t, "LINE") && isWord(peek(1), "INPUT") && peek(2).isSymbol("#"))
        {
            return parseLineInput();
        }
        // Name old As new: As never appears in an expression, so a statement
        // with it is this one (Name stays free as a variable or member).
        if (isWord(t, "NAME") && !peek(1).isSymbol("=") && !peek(1).isSymbol(".") &&
            !peek(1).isSymbol("(") && statementHas("AS"))
        {
            StmtPtr s = libraryStatement(next().location);
            ExprPtr call = libraryCall(Builtin::FileName, "Name", s->location);
            call->args.push_back(parseExpression());
            if (!expectKeyword("AS", "As"))
            {
                failed_ = true;
                return nullptr;
            }
            call->args.push_back(parseExpression());
            s->args.push_back(std::move(call));
            return s;
        }
        if ((isWord(t, "GET") || isWord(t, "PUT")) && peek(1).isSymbol("#"))
        {
            return parseGetPut(isWord(t, "GET"));
        }
        if (isWord(t, "SEEK") && peek(1).isSymbol("#"))
        {
            StmtPtr s = libraryStatement(next().location);
            next(); // #
            ExprPtr call = libraryCall(Builtin::FileSeek, "Seek #", s->location);
            call->args.push_back(parseExpression());
            if (!expectSymbol(","))
            {
                failed_ = true;
                return nullptr;
            }
            call->args.push_back(parseExpression());
            s->args.push_back(std::move(call));
            return s;
        }
        return nullptr;
    }

    static bool isWord(const Token &t, std::string_view upper)
    {
        if (t.kind != TokenKind::Identifier || t.suffix != 0 || t.text.size() != upper.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < upper.size(); ++i)
        {
            if (std::toupper(static_cast<unsigned char>(t.text[i])) != upper[i])
            {
                return false;
            }
        }
        return true;
    }

    // Does the statement from here hold the keyword?
    bool statementHas(std::string_view keyword) const
    {
        for (std::size_t ahead = 0;; ++ahead)
        {
            const Token &t = peek(ahead);
            if (t.kind == TokenKind::EndOfStatement || t.kind == TokenKind::EndOfFile)
            {
                return false;
            }
            if (t.isKeyword(keyword))
            {
                return true;
            }
        }
    }

    static ExprPtr libraryCall(Builtin id, const char *name, Location where)
    {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Call;
        e->builtin = id;
        e->text = name;
        e->location = where;
        return e;
    }

    static ExprPtr integer(std::int64_t value, Location where)
    {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::IntegerLiteral;
        e->integer = value;
        e->location = where;
        return e;
    }

    static StmtPtr libraryStatement(Location where)
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Library;
        s->location = where;
        return s;
    }

    // [#]number
    ExprPtr parseFileNumber()
    {
        acceptSymbol("#");
        return parseExpression();
    }

    // Open path For Input|Output|Append|Binary As [#]number
    StmtPtr parseOpen()
    {
        StmtPtr s = libraryStatement(next().location);
        ExprPtr call = libraryCall(Builtin::FileOpen, "Open", s->location);
        call->args.push_back(parseExpression());
        expectKeyword("FOR", "For");
        int mode = 0;
        const Token &m = peek();
        if (isWord(m, "INPUT"))
        {
            mode = 1;
        }
        else if (isWord(m, "OUTPUT"))
        {
            mode = 2;
        }
        else if (isWord(m, "APPEND"))
        {
            mode = 3;
        }
        else if (isWord(m, "BINARY"))
        {
            mode = 4;
        }
        if (mode == 0)
        {
            error(m.location, "expected Input, Output, Append or Binary but found " + describe(m));
            failed_ = true;
            return nullptr;
        }
        call->args.push_back(integer(mode, next().location));
        if (!expectKeyword("AS", "As"))
        {
            failed_ = true;
            return nullptr;
        }
        call->args.push_back(parseFileNumber());
        s->args.push_back(std::move(call));
        return s;
    }

    // Close (every file), or Close [#]n, [#]m, ...
    StmtPtr parseClose()
    {
        StmtPtr s = libraryStatement(next().location);
        if (atStatementEnd())
        {
            s->args.push_back(libraryCall(Builtin::FileCloseAll, "Close", s->location));
            return s;
        }
        do
        {
            ExprPtr call = libraryCall(Builtin::FileClose, "Close", peek().location);
            call->args.push_back(parseFileNumber());
            s->args.push_back(std::move(call));
        } while (acceptSymbol(","));
        return s;
    }

    // Print #n[, item {; item}[;]]: the items joined as & joins them; a
    // trailing ";" leaves out the newline. No comma zones (settled).
    StmtPtr parsePrint()
    {
        StmtPtr s = libraryStatement(next().location);
        next(); // #
        ExprPtr call = libraryCall(Builtin::FilePrint, "Print #", s->location);
        call->args.push_back(parseExpression());
        ExprPtr text; // item & item ... (sema makes a lone number text)
        bool newline = true;
        if (acceptSymbol(","))
        {
            while (!atStatementEnd())
            {
                ExprPtr item = parseExpression();
                const Location at = item->location;
                text = text == nullptr ? std::move(item)
                                       : makeBinary("&", at, std::move(text), std::move(item));
                newline = true;
                if (acceptSymbol(";"))
                {
                    newline = false;
                    continue;
                }
                if (peek().isSymbol(","))
                {
                    error(peek().location, "Print # separates items with ';', not ','");
                    failed_ = true;
                    return nullptr;
                }
                break;
            }
        }
        if (text == nullptr) // Print #n: just the newline
        {
            text = std::make_unique<Expr>();
            text->kind = ExprKind::StringLiteral;
            text->location = s->location;
        }
        call->args.push_back(std::move(text));
        call->args.push_back(integer(newline ? 1 : 0, s->location));
        s->args.push_back(std::move(call));
        return s;
    }

    // Get #n, [offset], buffer [, count] and Put the same: each optional part
    // goes to the runtime with a flag saying whether it was written.
    StmtPtr parseGetPut(bool get)
    {
        StmtPtr s = libraryStatement(next().location);
        next(); // #
        ExprPtr call = libraryCall(get ? Builtin::FileGet : Builtin::FilePut,
                                   get ? "Get #" : "Put #", s->location);
        call->args.push_back(parseExpression());
        if (!expectSymbol(","))
        {
            failed_ = true;
            return nullptr;
        }
        const bool hasOffset = !peek().isSymbol(",");
        call->args.push_back(integer(hasOffset ? 1 : 0, s->location));
        call->args.push_back(hasOffset ? parseExpression() : integer(0, s->location));
        if (!expectSymbol(","))
        {
            failed_ = true;
            return nullptr;
        }
        call->args.push_back(parsePrimary()); // the buffer: a Byte array
        const bool hasCount = acceptSymbol(",");
        call->args.push_back(integer(hasCount ? 1 : 0, s->location));
        call->args.push_back(hasCount ? parseExpression() : integer(0, s->location));
        s->args.push_back(std::move(call));
        return s;
    }

    // Line Input #n, target: target = the next line.
    StmtPtr parseLineInput()
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Assign;
        s->location = next().location;
        next(); // Input
        next(); // #
        ExprPtr call = libraryCall(Builtin::LineInput, "Line Input", s->location);
        call->args.push_back(parseExpression());
        if (!expectSymbol(","))
        {
            failed_ = true;
            return nullptr;
        }
        s->target = parsePrimary();
        s->value = std::move(call);
        return s;
    }

    // "x.Name = value", "a(i) = value"; or, with no "=", a method such as
    // "d.Add key, value" (only a Dictionary has methods).
    StmtPtr parseAssignOrMethod()
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Assign;
        s->location = peek().location;
        s->target = parsePrimary();
        if (acceptSymbol("="))
        {
            s->value = parseExpression();
            return s;
        }
        if (s->target->kind != ExprKind::Member && s->target->kind != ExprKind::Index)
        {
            expectSymbol("=");
            return nullptr;
        }
        s->kind = StmtKind::Method;
        if (!atStatementEnd())
        {
            parseArguments(s->args);
        }
        return s;
    }

    // End of statement, or Else ending the Then part of a single-line If.
    bool atStatementEnd(std::size_t ahead = 0) const
    {
        const Token &t = peek(ahead);
        return t.kind == TokenKind::EndOfStatement || t.kind == TokenKind::EndOfFile ||
               t.isKeyword("ELSE");
    }

    // "name", "name a, b", "name(a, b)". Parentheses around the whole list
    // are only grouping here, so "Show(a, b)" and "Show a, b" are the same
    // call; unlike VB6 they never turn a ByRef argument into a ByVal one.
    StmtPtr parseCallStatement()
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Call;
        s->location = peek().location;
        s->callee = next().text;
        if (atStatementEnd())
        {
            return s;
        }
        if (peek().isSymbol("(") && parenthesisedToEnd())
        {
            next();
            parseArguments(s->args);
            expectSymbol(")");
            return s;
        }
        parseArguments(s->args);
        return s;
    }

    // At the '(' `ahead` tokens on: is its matching ')' followed by symbol?
    // "a(i) = 5" is an assignment to an array element, not a call.
    bool parenthesisedThen(std::size_t ahead, std::string_view symbol) const
    {
        int depth = 0;
        for (;; ++ahead)
        {
            const Token &t = peek(ahead);
            if (t.kind == TokenKind::EndOfStatement || t.kind == TokenKind::EndOfFile)
            {
                return false;
            }
            if (t.isSymbol("("))
            {
                ++depth;
            }
            else if (t.isSymbol(")") && --depth == 0)
            {
                return peek(ahead + 1).isSymbol(symbol);
            }
        }
    }

    // At '(': does its matching ')' end the statement? Distinguishes
    // "Show(a, b)" from "Show (a + 1) * 2, b".
    bool parenthesisedToEnd() const
    {
        int depth = 0;
        for (std::size_t ahead = 0;; ++ahead)
        {
            const Token &t = peek(ahead);
            if (t.kind == TokenKind::EndOfStatement || t.kind == TokenKind::EndOfFile)
            {
                return false;
            }
            if (t.isSymbol("("))
            {
                ++depth;
            }
            else if (t.isSymbol(")") && --depth == 0)
            {
                return atStatementEnd(ahead + 1);
            }
        }
    }

    // Comma-separated expressions; an empty list when at ')'.
    void parseArguments(std::vector<ExprPtr> &args)
    {
        if (peek().isSymbol(")"))
        {
            return;
        }
        do
        {
            args.push_back(parseExpression());
        } while (acceptSymbol(","));
    }

    StmtPtr parseExit()
    {
        static const std::pair<const char *, const char *> kTargets[] = {{"FOR", "For"},
                                                                         {"DO", "Do"},
                                                                         {"WHILE", "While"},
                                                                         {"SUB", "Sub"},
                                                                         {"FUNCTION", "Function"}};
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Exit;
        s->location = next().location; // EXIT
        for (const auto &[keyword, name] : kTargets)
        {
            if (acceptKeyword(keyword))
            {
                s->exitWhat = name;
                return s;
            }
        }
        error(peek().location, "expected 'For', 'Do', 'While', 'Sub' or 'Function' after 'Exit'");
        return nullptr;
    }

    StmtPtr parseFor()
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::For;
        s->location = next().location; // FOR

        s->isForEach = acceptKeyword("EACH");
        auto var = std::make_unique<Expr>();
        var->kind = ExprKind::Name;
        var->location = peek().location;
        var->suffix = peek().suffix;
        var->text = expectIdentifier("a loop variable");
        if (peek().isKeyword("AS"))
        {
            declareLoopVariable(*s, *var);
        }
        s->target = std::move(var);
        if (s->isForEach)
        {
            expectKeyword("IN", "In");
            s->value = parseExpression(); // the array
        }
        else
        {
            expectSymbol("=");
            s->value = parseExpression();
            expectKeyword("TO", "To");
            s->limit = parseExpression();
            if (acceptKeyword("STEP"))
            {
                s->step = parseExpression();
            }
        }
        expectEndOfStatement();

        s->body = parseBlock("FOR");

        s->nextLocation = peek().location;
        if (expectKeyword("NEXT", "Next"))
        {
            if (at(TokenKind::Identifier))
            {
                s->nextName = next().text;
            }
            expectEndOfStatement();
        }
        return s;
    }

    // For i As Integer = ... / For Each w As String In ...: a local of the
    // procedure, which the semantic pass scopes to the loop.
    void declareLoopVariable(Stmt &s, const Expr &name)
    {
        const Location as = next().location; // AS
        auto v = std::make_unique<Variable>();
        v->name = name.text;
        v->location = name.location;
        std::string enumName;
        bool isNew = false;
        const Type declared = parseTypeName(&enumName, &isNew);
        if (dictionary_)
        {
            dictionary_ = false;
            error(as, "a loop variable cannot be a Dictionary");
        }
        if (isNew)
        {
            error(as, "a loop variable is given its values by the loop: write 'As " + enumName +
                          "' without New");
        }
        const Type suffix = typeFromSuffix(name.suffix);
        if (suffix != Type::Error && suffix != declared && declared != Type::Error)
        {
            error(as, "'" + v->name + "' has a type suffix that contradicts 'As " +
                          typeName(declared) + "'");
        }
        v->type = declared;
        v->typeName = enumName;
        if (currentProcedure_ == nullptr)
        {
            return;
        }
        s.loopVariable = v.get();
        currentProcedure_->locals.push_back(std::move(v));
    }

    StmtPtr parseDo()
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Do;
        s->location = next().location; // DO
        if (peek().isKeyword("WHILE") || peek().isKeyword("UNTIL"))
        {
            s->until = next().isKeyword("UNTIL");
            s->testAtTop = true;
            s->condition = parseExpression();
        }
        expectEndOfStatement();

        s->body = parseBlock("DO");

        s->loopLocation = peek().location;
        if (!expectKeyword("LOOP", "Loop"))
        {
            return s; // the closer belongs to an outer construct
        }
        if (peek().isKeyword("WHILE") || peek().isKeyword("UNTIL"))
        {
            const Token &test = next();
            if (s->testAtTop)
            {
                error(test.location, "a Do loop tests its condition at the top or at the "
                                     "bottom, not both");
                parseExpression();
            }
            else
            {
                s->until = test.isKeyword("UNTIL");
                s->condition = parseExpression();
            }
        }
        expectEndOfStatement();
        return s;
    }

    // While ... Wend (VB-DOS) and While ... End While (VB.NET) are the same
    // loop: a Do While with the test at the top.
    StmtPtr parseWhile()
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Do;
        s->isWhile = true;
        s->testAtTop = true;
        s->location = next().location; // WHILE
        s->condition = parseExpression();
        expectEndOfStatement();

        s->body = parseBlock("WHILE");

        s->loopLocation = peek().location;
        if (isEndOf("WHILE"))
        {
            next();
            next();
            expectEndOfStatement();
        }
        else if (expectKeyword("WEND", "Wend' or 'End While"))
        {
            expectEndOfStatement();
        }
        return s;
    }

    StmtPtr parseTry()
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Try;
        s->location = next().location; // TRY
        expectEndOfStatement();
        s->body = parseBlock("TRY");

        while (peek().isKeyword("CATCH"))
        {
            CatchClause clause;
            clause.location = next().location;
            if (at(TokenKind::Identifier))
            {
                // VB.NET's "Catch ex As Exception": there are no exception
                // objects here.
                error(peek().location, "Catch takes no variable: the error is in Err "
                                       "(write Catch, or Catch When Err.Number = ...)");
                while (!at(TokenKind::EndOfStatement) && !at(TokenKind::EndOfFile) &&
                       !peek().isKeyword("WHEN"))
                {
                    next();
                }
            }
            if (acceptKeyword("WHEN"))
            {
                clause.when = parseExpression();
            }
            expectEndOfStatement();
            clause.body = parseBlock("TRY");
            s->catches.push_back(std::move(clause));
        }
        if (peek().isKeyword("FINALLY"))
        {
            s->hasFinally = true;
            s->finallyLocation = next().location;
            expectEndOfStatement();
            s->finallyBody = parseBlock("TRY");
        }
        if (s->catches.empty() && !s->hasFinally)
        {
            error(s->location, "a Try needs at least one Catch or a Finally");
        }

        s->endLocation = peek().location;
        if (expectEndOf("TRY", "End Try"))
        {
            expectEndOfStatement();
        }
        return s;
    }

    // With target EOS block End With
    StmtPtr parseWith()
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::With;
        s->location = next().location;
        s->target = parseExpression();
        expectEndOfStatement();
        s->body = parseBlock("WITH");
        s->endLocation = peek().location;
        if (expectEndOf("WITH", "End With"))
        {
            expectEndOfStatement();
        }
        return s;
    }

    // Select Case value EOS { Case tests EOS block } [Case Else EOS block]
    // End Select, where a test is a value, "a To b" or "Is <op> value".
    StmtPtr parseSelect()
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Select;
        s->location = next().location; // SELECT
        expectKeyword("CASE", "Case");
        s->value = parseExpression();
        expectEndOfStatement();
        skipStatementEnds();
        if (!peek().isKeyword("CASE") && !isEndOf("SELECT") && !at(TokenKind::EndOfFile))
        {
            error(peek().location,
                  "expected 'Case' after 'Select Case' but found " + describe(peek()));
            skipStatement();
            skipStatementEnds();
        }
        while (peek().isKeyword("CASE"))
        {
            const Location where = next().location;
            if (s->hasElse)
            {
                error(where, "'Case Else' must be the last Case");
            }
            if (acceptKeyword("ELSE"))
            {
                s->hasElse = true;
                expectEndOfStatement();
                s->elseBody = parseBlock("SELECT");
                continue;
            }
            CaseClause clause;
            clause.location = where;
            do
            {
                clause.tests.push_back(parseCaseTest());
            } while (acceptSymbol(","));
            expectEndOfStatement();
            clause.body = parseBlock("SELECT");
            s->cases.push_back(std::move(clause));
        }
        s->endLocation = peek().location;
        if (expectEndOf("SELECT", "End Select"))
        {
            expectEndOfStatement();
        }
        return s;
    }

    CaseTest parseCaseTest()
    {
        CaseTest test;
        test.location = peek().location;
        if (acceptKeyword("IS"))
        {
            for (const char *op : {"=", "<>", "<", ">", "<=", ">="})
            {
                if (peek().isSymbol(op))
                {
                    next();
                    test.op = op;
                    break;
                }
            }
            if (test.op.empty())
            {
                error(peek().location, "expected a comparison (=, <>, <, >, <=, >=) after 'Is'");
                test.op = "=";
            }
            test.value = parseExpression();
            return test;
        }
        for (const char *op : {"=", "<>", "<", ">", "<=", ">="})
        {
            if (peek().isSymbol(op))
            {
                error(peek().location,
                      std::string("a comparison in a Case is written 'Is ") + op + " value'");
                next();
                test.op = op;
                test.value = parseExpression();
                return test;
            }
        }
        test.value = parseExpression();
        if (acceptKeyword("TO"))
        {
            test.op = "To";
            test.upper = parseExpression();
        }
        return test;
    }

    // Const NAME [As type] = value, ...; the type may also come from a suffix,
    // or, with neither, from the value (worked out by the semantic pass).
    StmtPtr parseConst(Module *module, Procedure *procedure)
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Const;
        s->location = next().location; // CONST
        for (;;)
        {
            auto v = std::make_unique<Variable>();
            v->isConst = true;
            v->isModuleLevel = module != nullptr;
            v->location = peek().location;
            const char suffix = peek().suffix;
            v->name = expectIdentifier("a constant name");
            if (v->name.empty())
            {
                break;
            }
            v->type = typeFromSuffix(suffix);
            if (acceptKeyword("AS"))
            {
                const Location typeAt = peek().location;
                const Type declared = parseTypeName(&v->typeName);
                if (dictionary_)
                {
                    dictionary_ = false;
                    error(typeAt, "a Const cannot be a Dictionary");
                }
                if (v->type != Type::Error && declared != Type::Error && v->type != declared)
                {
                    error(typeAt, "'" + v->name + "' has a type suffix that contradicts 'As " +
                                      typeName(declared) + "'");
                }
                v->type = declared;
            }
            if (!expectSymbol("="))
            {
                break;
            }
            s->constValues.push_back(parseExpression());
            std::vector<std::unique_ptr<Variable>> one;
            one.push_back(std::move(v));
            flush(one, *s, module, procedure);
            if (!acceptSymbol(","))
            {
                break;
            }
        }
        return s;
    }

    // Type Name, its members one per line (name [shape] As type), End Type.
    void parseType(Module &module)
    {
        auto def = std::make_unique<TypeDef>();
        def->location = next().location; // TYPE
        def->name = expectIdentifier("a Type name");
        expectEndOfStatement();
        skipStatementEnds();
        while (!at(TokenKind::EndOfFile) && !isEndOf("TYPE") && !isEndOf("SUB") &&
               !isEndOf("FUNCTION") && !peek().isKeyword("SUB") && !peek().isKeyword("FUNCTION"))
        {
            if (!at(TokenKind::Identifier) && !at(TokenKind::Keyword))
            {
                error(peek().location, "expected a member name but found " + describe(peek()));
                skipStatement();
                skipStatementEnds();
                continue;
            }
            auto member = std::make_unique<Variable>();
            member->isMember = true;
            member->location = peek().location;
            const char suffix = peek().suffix;
            member->name = expectMemberName();
            member->type = typeFromSuffix(suffix);
            if (peek().isSymbol("("))
            {
                parseShape(*member, true);
            }
            parseOptionalAs(*member, "member '" + member->name + "'");
            def->members.push_back(std::move(member));
            expectEndOfStatement();
            skipStatementEnds();
        }
        if (expectEndOf("TYPE", "End Type"))
        {
            expectEndOfStatement();
        }
        module.types.push_back(std::move(def));
    }

    // Enum Name [As Integer | Long], its members one per line, End Enum.
    void parseEnum(Module &module)
    {
        auto def = std::make_unique<EnumDef>();
        def->location = next().location; // ENUM
        def->name = expectIdentifier("an Enum name");
        if (acceptKeyword("AS"))
        {
            const Location typeAt = peek().location;
            def->underlying = parseTypeName();
            if (def->underlying != Type::Integer && def->underlying != Type::Long)
            {
                error(typeAt, "an Enum is based on Integer or Long");
                def->underlying = Type::Integer;
            }
        }
        expectEndOfStatement();
        skipStatementEnds();
        while (!at(TokenKind::EndOfFile) && !isEndOf("ENUM") && !isEndOf("SUB") &&
               !isEndOf("FUNCTION") && !peek().isKeyword("SUB") && !peek().isKeyword("FUNCTION"))
        {
            if (!at(TokenKind::Identifier))
            {
                error(peek().location,
                      "expected an Enum member name but found " + describe(peek()));
                skipStatement();
                skipStatementEnds();
                continue;
            }
            EnumMember member;
            member.location = peek().location;
            member.name = next().text;
            if (acceptSymbol("="))
            {
                member.value = parseExpression();
            }
            def->members.push_back(std::move(member));
            expectEndOfStatement();
            skipStatementEnds();
        }
        if (expectEndOf("ENUM", "End Enum"))
        {
            expectEndOfStatement();
        }
        module.enums.push_back(std::move(def));
    }

    // Public x As T, Private Sub ..., Public Const ...: the declaration
    // after the word, marked.
    void parseVisible(Module &module)
    {
        const bool isPublic = next().isKeyword("PUBLIC");
        const Token &t = peek();
        if (t.isKeyword("SUB") || t.isKeyword("FUNCTION"))
        {
            module.procedures.push_back(parseProcedure());
            module.procedures.back()->isPrivate = !isPublic;
            return;
        }
        if (t.isKeyword("ENUM") || t.isKeyword("TYPE"))
        {
            const bool isEnum = t.isKeyword("ENUM");
            const std::size_t before = isEnum ? module.enums.size() : module.types.size();
            isEnum ? parseEnum(module) : parseType(module);
            if (isEnum && module.enums.size() > before)
            {
                module.enums.back()->isPrivate = !isPublic;
            }
            else if (!isEnum && module.types.size() > before)
            {
                module.types.back()->isPrivate = !isPublic;
            }
            return;
        }
        StmtPtr s;
        if (t.isKeyword("CONST"))
        {
            s = parseConst(&module, nullptr);
        }
        else if (t.isKeyword("DIM"))
        {
            error(t.location, "write 'Public x As ...' or 'Private x As ...', without Dim");
            skipStatement();
            return;
        }
        else if (t.kind == TokenKind::Identifier)
        {
            s = parseDim(&module, nullptr, false); // Public x As T: a Dim
        }
        else
        {
            error(t.location, "expected a declaration after '" +
                                  std::string(isPublic ? "Public" : "Private") + "' but found " +
                                  describe(t));
            skipStatement();
            return;
        }
        for (Variable *v : s->declared)
        {
            v->isPublic = isPublic;
        }
        module.moduleStatements.push_back(std::move(s));
        expectEndOfStatement();
    }

    // Dim a, b As T (or, with dimWord false, the names after Public/Private).
    StmtPtr parseDim(Module *module, Procedure *procedure, bool dimWord = true)
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Dim;
        s->location = dimWord ? next().location : peek().location; // DIM

        // "Dim a, b As String" declares both as String (VB.NET rules).
        std::vector<std::unique_ptr<Variable>> pending;
        for (;;)
        {
            auto v = std::make_unique<Variable>();
            v->location = peek().location;
            const char suffix = peek().suffix;
            v->name = expectIdentifier("a variable name");
            if (v->name.empty())
            {
                break; // the caller's end-of-statement check resynchronises
            }
            v->type = typeFromSuffix(suffix);
            v->isModuleLevel = module != nullptr;
            if (peek().isSymbol("("))
            {
                parseShape(*v, true);
            }
            pending.push_back(std::move(v));

            if (acceptKeyword("AS"))
            {
                const Location typeAt = peek().location;
                std::string enumName;
                bool isNew = false;
                const Type declared = parseTypeName(&enumName, &isNew);
                const bool dictionary = dictionary_;
                dictionary_ = false;
                // A value after this As may go only with a name that is its
                // sole user (the others have suffixes of their own).
                const bool alone =
                    std::all_of(pending.begin(), pending.end() - 1,
                                [](const auto &p) { return p->type != Type::Error; });
                // "As" types the names before it that have no suffix of their
                // own; only the name written directly before it may not
                // contradict it ("Dim n% As String").
                for (std::size_t i = 0; i < pending.size(); ++i)
                {
                    Variable &p = *pending[i];
                    const bool adjacent = i + 1 == pending.size();
                    if (p.type == Type::Error)
                    {
                        p.type = declared;
                        p.typeName = enumName;
                        p.autoNew = isNew;
                        if (dictionary)
                        {
                            dictionary_ = true;
                            takeDictionary(p, typeAt);
                        }
                    }
                    else if (adjacent && p.type != declared && declared != Type::Error)
                    {
                        error(typeAt, "'" + p.name + "' has a type suffix that contradicts 'As " +
                                          typeName(declared) + "'");
                    }
                }
                Variable *last = pending.back().get();
                flush(pending, *s, module, procedure);
                if (peek().isSymbol("="))
                {
                    parseInitialiser(*s, *last, alone);
                }
            }
            else if (peek().isSymbol("="))
            {
                Variable *last = pending.back().get();
                const bool alone = pending.size() == 1;
                flush(pending, *s, module, procedure);
                parseInitialiser(*s, *last, alone);
            }
            if (!acceptSymbol(","))
            {
                break;
            }
        }

        // Names with neither suffix nor As: there are no Variants to fall back on.
        for (auto &p : pending)
        {
            if (p->type == Type::Error)
            {
                error(p->location,
                      "'" + p->name + "' needs a type, such as 'As Integer' or 'As String'");
            }
        }
        flush(pending, *s, module, procedure);
        return s;
    }

    // "= value" after a declared name: lowered into statements in the
    // Dim's body (see Stmt::declared). alone: the name has its own type
    // ("Dim a, b As Integer = 5" gives one As to two names, as VB.NET
    // forbids).
    void parseInitialiser(Stmt &dim, Variable &v, bool alone)
    {
        const Location where = next().location; // =
        if (v.type == Type::Error)
        {
            error(v.location, "'" + v.name + "' needs a type, such as 'As Integer' or " +
                                  "'As String' (a value does not give it one)");
        }
        else if (!alone)
        {
            error(where, "a name given a value needs its own type: write 'Dim " + v.name +
                             " As <type> = value' (other names in the Dim take theirs)");
        }
        else if (v.autoNew)
        {
            error(where, "'As New' already gives '" + v.name + "' a value");
        }
        else if (v.isDictionary)
        {
            error(where, "a Dictionary starts empty: fill it after the Dim");
        }
        if (!peek().isSymbol("{"))
        {
            if (v.rank > 0 && !v.isDynamic)
            {
                error(where, "'" + v.name +
                                 "' has a fixed size; only a dynamic array, "
                                 "'Dim " +
                                 v.name + "() As ...', takes a value");
            }
            dim.body.push_back(assignTo(v, nullptr, parseExpression(), where));
            return;
        }
        // { ... } (nested for more dimensions): ReDim to its shape, then each
        // element in turn.
        const Location listAt = peek().location;
        std::vector<std::pair<std::vector<std::int64_t>, ExprPtr>> items;
        std::vector<std::int64_t> shape;
        std::vector<std::int64_t> index;
        parseList(items, shape, index, 0);
        if (v.rank == 0)
        {
            error(listAt, "a list in braces gives an array its values, but '" + v.name +
                              "' is not an array");
            return;
        }
        if (!v.isDynamic)
        {
            error(where, "'" + v.name + "' has a fixed size; only a dynamic array, 'Dim " + v.name +
                             "() As ...', takes a list");
            return;
        }
        if (static_cast<int>(shape.size()) != v.rank)
        {
            error(listAt, "'" + v.name + "' has " + std::to_string(v.rank) +
                              (v.rank == 1 ? " dimension" : " dimensions") + " but the list has " +
                              std::to_string(shape.size()) + " (one level of braces each)");
            return;
        }
        if (items.empty())
        {
            return; // {}: the array stays empty
        }
        auto redim = std::make_unique<Stmt>();
        redim->kind = StmtKind::ReDim;
        redim->location = where;
        std::vector<ExprPtr> bounds;
        for (std::int64_t n : shape)
        {
            bounds.push_back(integer(n - 1, listAt));
        }
        redim->args.push_back(arrayCall(v, std::move(bounds), listAt));
        dim.body.push_back(std::move(redim));
        for (auto &[at, value] : items)
        {
            std::vector<ExprPtr> subscripts;
            for (std::int64_t i : at)
            {
                subscripts.push_back(integer(i, value->location));
            }
            const Location valueAt = value->location;
            dim.body.push_back(assignTo(v, arrayCall(v, std::move(subscripts), valueAt),
                                        std::move(value), valueAt));
        }
    }

    // { item, ... }: items are values, or lists one level down. Every list
    // at a level must be as long as the first (rows of equal length).
    void parseList(std::vector<std::pair<std::vector<std::int64_t>, ExprPtr>> &items,
                   std::vector<std::int64_t> &shape, std::vector<std::int64_t> &index,
                   std::size_t level)
    {
        const Location open = next().location; // {
        if (shape.size() <= level)
        {
            shape.push_back(-1);
        }
        std::int64_t count = 0;
        if (!peek().isSymbol("}"))
        {
            do
            {
                index.push_back(count);
                if (peek().isSymbol("{"))
                {
                    parseList(items, shape, index, level + 1);
                }
                else if (shape.size() > level + 1)
                {
                    error(peek().location, "expected a list in braces here, as in the rows "
                                           "before it");
                    parseExpression();
                }
                else
                {
                    items.emplace_back(index, parseExpression());
                }
                index.pop_back();
                ++count;
            } while (acceptSymbol(","));
        }
        expectSymbol("}");
        if (shape[level] == -1)
        {
            shape[level] = count;
        }
        else if (shape[level] != count)
        {
            error(open, "this list has " + std::to_string(count) +
                            (count == 1 ? " item" : " items") + " but the one before it has " +
                            std::to_string(shape[level]) +
                            " (an array's rows are all the same length)");
        }
    }

    // name = value, or name(subscripts) = value when element is given.
    static StmtPtr assignTo(const Variable &v, ExprPtr element, ExprPtr value, Location where)
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Assign;
        s->location = where;
        if (element != nullptr)
        {
            s->target = std::move(element);
        }
        else
        {
            auto name = std::make_unique<Expr>();
            name->kind = ExprKind::Name;
            name->location = v.location;
            name->text = v.name;
            s->target = std::move(name);
        }
        s->value = std::move(value);
        return s;
    }

    // name(args), as parsePrimary gives it.
    static ExprPtr arrayCall(const Variable &v, std::vector<ExprPtr> args, Location where)
    {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Call;
        e->location = where;
        e->text = v.name;
        e->args = std::move(args);
        return e;
    }

    static Type typeFromSuffix(char suffix)
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

    // A built-in type keyword, or an Enum's name (stored in *enumName for the
    // semantic pass to resolve; without enumName, only built-in types).
    Type parseTypeName(std::string *enumName = nullptr, bool *isNew = nullptr)
    {
        // Dictionary(Of T): T is returned, and dictionary_ set for the
        // caller to take (takeDictionary) or reject.
        if (enumName != nullptr && isNew != nullptr && peek().isKeyword("NEW") &&
            peek(1).text == "Dictionary")
        {
            error(next().location, "a Dictionary is ready to use when declared: write "
                                   "'As Dictionary(Of ...)' without New");
        }
        if (enumName != nullptr && at(TokenKind::Identifier) && peek().text == "Dictionary" &&
            peek(1).isSymbol("("))
        {
            next();
            next();
            expectKeyword("OF", "Of");
            if (at(TokenKind::Identifier) && peek().text == "Dictionary")
            {
                error(peek().location, "a Dictionary's values cannot be Dictionaries; put the "
                                       "inner Dictionary in a Type");
            }
            const Type values = parseTypeName(enumName);
            expectSymbol(")");
            dictionary_ = true;
            return values;
        }
        // Ref Customer; New Customer (in a Dim: a Ref given a new record).
        const bool newOne = isNew != nullptr && acceptKeyword("NEW");
        if (enumName != nullptr && (newOne || acceptKeyword("REF")))
        {
            if (isNew != nullptr)
            {
                *isNew = newOne;
            }
            *enumName = expectIdentifier("a Type name");
            return Type::Ref;
        }
        if (enumName != nullptr && at(TokenKind::Identifier))
        {
            *enumName = next().text;
            return Type::Enum;
        }
        if (acceptKeyword("INTEGER"))
        {
            return Type::Integer;
        }
        if (acceptKeyword("BYTE"))
        {
            return Type::Byte;
        }
        if (acceptKeyword("STRING"))
        {
            return Type::String;
        }
        if (acceptKeyword("BOOLEAN"))
        {
            return Type::Boolean;
        }
        if (acceptKeyword("LONG"))
        {
            return Type::Long;
        }
        if (acceptKeyword("DOUBLE"))
        {
            return Type::Double;
        }
        error(peek().location, "expected a type but found " + describe(peek()));
        return Type::Error;
    }

    // After parseTypeName: if it was Dictionary(Of T), v becomes one (a
    // dynamic array of rank 1 underneath).
    void takeDictionary(Variable &v, Location where)
    {
        if (!dictionary_)
        {
            return;
        }
        dictionary_ = false;
        if (v.rank > 0)
        {
            error(where, "an array of Dictionaries is not available; put the Dictionary in a "
                         "Type and make an array of that");
            return;
        }
        v.isDictionary = true;
        v.rank = 1;
        v.isDynamic = true;
    }

    // Moves declared variables to their owner and records them on the Dim.
    static void flush(std::vector<std::unique_ptr<Variable>> &pending, Stmt &dim, Module *module,
                      Procedure *procedure)
    {
        for (auto &v : pending)
        {
            dim.declared.push_back(v.get());
            if (module != nullptr)
            {
                module->globals.push_back(std::move(v));
            }
            else if (procedure != nullptr)
            {
                procedure->locals.push_back(std::move(v));
            }
        }
        pending.clear();
    }

    StmtPtr parseIf()
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::If;
        s->location = next().location; // IF

        IfArm first;
        first.condition = parseExpression();
        expectKeyword("THEN", "Then");

        // Single-line form: "If x Then stmt [Else stmt]".
        if (!at(TokenKind::EndOfStatement))
        {
            first.body.push_back(parseSimpleStatement());
            s->arms.push_back(std::move(first));
            if (acceptKeyword("ELSE"))
            {
                s->hasElse = true;
                s->elseBody.push_back(parseSimpleStatement());
            }
            expectEndOfStatement();
            return s;
        }

        first.body = parseBlock("IF");
        s->arms.push_back(std::move(first));

        while (peek().isKeyword("ELSEIF"))
        {
            next();
            IfArm arm;
            arm.condition = parseExpression();
            expectKeyword("THEN", "Then");
            expectEndOfStatement();
            arm.body = parseBlock("IF");
            s->arms.push_back(std::move(arm));
        }
        if (acceptKeyword("ELSE"))
        {
            s->hasElse = true;
            expectEndOfStatement();
            s->elseBody = parseBlock("IF");
        }
        if (expectEndOf("IF", "End If"))
        {
            expectEndOfStatement();
        }
        return s;
    }

    // -- Expressions -------------------------------------------------------

    ExprPtr makeBinary(std::string op, Location where, ExprPtr left, ExprPtr right)
    {
        auto e = std::make_unique<Expr>();
        e->kind = ExprKind::Binary;
        e->location = where;
        e->text = std::move(op);
        e->left = std::move(left);
        e->right = std::move(right);
        return e;
    }

    ExprPtr parseExpression() { return parseOr(); }

    ExprPtr parseOr()
    {
        ExprPtr left = parseAnd();
        while (peek().isKeyword("OR"))
        {
            const Location where = next().location;
            left = makeBinary("Or", where, std::move(left), parseAnd());
        }
        return left;
    }

    ExprPtr parseAnd()
    {
        ExprPtr left = parseNot();
        while (peek().isKeyword("AND"))
        {
            const Location where = next().location;
            left = makeBinary("And", where, std::move(left), parseNot());
        }
        return left;
    }

    ExprPtr parseNot()
    {
        if (peek().isKeyword("NOT"))
        {
            auto e = std::make_unique<Expr>();
            e->kind = ExprKind::Unary;
            e->location = next().location;
            e->text = "Not";
            e->left = parseNot();
            return e;
        }
        return parseComparison();
    }

    ExprPtr parseComparison()
    {
        ExprPtr left = parseConcat();
        for (const char *op : {"=", "<>", "<", ">", "<=", ">="})
        {
            if (peek().isSymbol(op))
            {
                const Location where = next().location;
                return makeBinary(op, where, std::move(left), parseConcat());
            }
        }
        // r Is Nothing, r IsNot s: Refs compared by identity.
        if (peek().isKeyword("IS") || peek().isKeyword("ISNOT"))
        {
            const Token &op = next();
            return makeBinary(op.isKeyword("IS") ? "Is" : "IsNot", op.location, std::move(left),
                              parseConcat());
        }
        return left;
    }

    ExprPtr parseConcat()
    {
        ExprPtr left = parseAdditive();
        while (peek().isSymbol("&"))
        {
            const Location where = next().location;
            left = makeBinary("&", where, std::move(left), parseAdditive());
        }
        return left;
    }

    ExprPtr parseAdditive()
    {
        ExprPtr left = parseMod();
        while (peek().isSymbol("+") || peek().isSymbol("-"))
        {
            const Token &op = next();
            left = makeBinary(op.text, op.location, std::move(left), parseMod());
        }
        return left;
    }

    ExprPtr parseMod()
    {
        ExprPtr left = parseIntegerDivision();
        while (peek().isKeyword("MOD"))
        {
            const Location where = next().location;
            left = makeBinary("Mod", where, std::move(left), parseIntegerDivision());
        }
        return left;
    }

    ExprPtr parseIntegerDivision()
    {
        ExprPtr left = parseMultiplicative();
        while (peek().isSymbol("\\"))
        {
            const Location where = next().location;
            left = makeBinary("\\", where, std::move(left), parseMultiplicative());
        }
        return left;
    }

    ExprPtr parseMultiplicative()
    {
        ExprPtr left = parseUnary();
        while (peek().isSymbol("*") || peek().isSymbol("/"))
        {
            const Token &op = next();
            left = makeBinary(op.text, op.location, std::move(left), parseUnary());
        }
        return left;
    }

    ExprPtr parseUnary()
    {
        if (peek().isSymbol("-"))
        {
            auto e = std::make_unique<Expr>();
            e->kind = ExprKind::Unary;
            e->location = next().location;
            e->text = "-";
            e->left = parseUnary();
            return e;
        }
        return parsePower();
    }

    // ^ binds tighter than unary minus (-2 ^ 2 is -4) and groups to the
    // left (2 ^ 3 ^ 2 is 64), as in VB; its right operand may be negated.
    ExprPtr parsePower()
    {
        ExprPtr left = parsePrimary();
        while (peek().isSymbol("^"))
        {
            const Location where = next().location;
            ExprPtr right;
            if (peek().isSymbol("-"))
            {
                right = std::make_unique<Expr>();
                right->kind = ExprKind::Unary;
                right->location = next().location;
                right->text = "-";
                right->left = parsePrimary();
            }
            else
            {
                right = parsePrimary();
            }
            left = makeBinary("^", where, std::move(left), std::move(right));
        }
        return left;
    }

    ExprPtr parsePrimary()
    {
        auto e = std::make_unique<Expr>();
        const Token &t = peek();
        e->location = t.location;

        switch (t.kind)
        {
            case TokenKind::IntegerLiteral:
                e->kind = ExprKind::IntegerLiteral;
                e->suffix = t.suffix;
                e->integer = next().integer;
                return e;
            case TokenKind::DoubleLiteral:
                e->kind = ExprKind::DoubleLiteral;
                e->real = next().real;
                return e;
            case TokenKind::StringLiteral:
                e->kind = ExprKind::StringLiteral;
                e->text = next().text;
                return e;
            case TokenKind::Identifier:
            {
                e->kind = ExprKind::Name;
                e->suffix = t.suffix;
                e->text = next().text;
                if (acceptSymbol("("))
                {
                    e->kind = ExprKind::Call; // a call, or an array element: sema decides
                    parseArguments(e->args);
                    expectSymbol(")");
                }
                return parsePostfix(std::move(e));
            }
            default:
                break;
        }

        if (t.isKeyword("NEW"))
        {
            e->kind = ExprKind::New;
            next();
            e->text = expectIdentifier("a Type name");
            return parsePostfix(std::move(e));
        }
        if (t.isKeyword("NOTHING"))
        {
            e->kind = ExprKind::Nothing;
            next();
            return e;
        }
        if (t.isKeyword("ME")) // the form of this form's module
        {
            e->kind = ExprKind::Name;
            e->text = "Me";
            next();
            return parsePostfix(std::move(e));
        }
        if (t.isKeyword("CTYPE"))
        {
            e->kind = ExprKind::Call;
            e->text = "CType";
            next();
            expectSymbol("(");
            e->args.push_back(parseExpression());
            expectSymbol(",");
            e->convertTo = parseTypeName(&e->convertToName);
            if (dictionary_)
            {
                dictionary_ = false;
                error(e->location, "CType cannot convert to a Dictionary");
            }
            expectSymbol(")");
            return e;
        }
        if (t.isKeyword("TRUE") || t.isKeyword("FALSE"))
        {
            e->kind = ExprKind::BooleanLiteral;
            e->integer = next().isKeyword("TRUE") ? 1 : 0;
            return e;
        }
        if (acceptSymbol("("))
        {
            ExprPtr inner = parseExpression();
            expectSymbol(")");
            return inner;
        }
        if (t.isSymbol("."))
        {
            return parsePostfix(nullptr); // .Name inside a With: sema supplies the object
        }

        error(t.location, "expected an expression but found " + describe(t));
        e->kind = ExprKind::IntegerLiteral; // placeholder so later stages have a node
        e->type = Type::Error;
        return e;
    }

    // ".Name" and "(i)" after a primary: o.Home.City, a(i).Lines(j),
    // txtName.Text. A "(...)" after a member indexes that member (an array).
    ExprPtr parsePostfix(ExprPtr e)
    {
        while (peek().isSymbol("."))
        {
            next();
            auto member = std::make_unique<Expr>();
            member->kind = ExprKind::Member;
            member->location = peek().location;
            member->text = expectMemberName();
            member->left = std::move(e);
            e = std::move(member);
            if (peek().isSymbol("("))
            {
                auto index = std::make_unique<Expr>();
                index->kind = ExprKind::Index;
                index->location = e->location;
                next();
                parseArguments(index->args);
                expectSymbol(")");
                index->left = std::move(e);
                e = std::move(index);
            }
        }
        return e;
    }

    const std::vector<Token> &tokens_;
    Diagnostics &diagnostics_;
    std::size_t pos_ = 0;
    int lastErrorLine_ = 0;
    Procedure *currentProcedure_ = nullptr;
    bool dictionary_ = false;        // parseTypeName read Dictionary(Of T)
    bool failed_ = false;            // parseFileStatement reported an error
    std::vector<const char *> open_; // constructs around the current block
};

} // namespace

const char *ast::typeName(Type type)
{
    switch (type)
    {
        case Type::Integer:
            return "Integer";
        case Type::Long:
            return "Long";
        case Type::Double:
            return "Double";
        case Type::Byte:
            return "Byte";
        case Type::Enum:
            return "Enum";
        case Type::Record:
            return "Type";
        case Type::Ref:
            return "Ref";
        case Type::String:
            return "String";
        case Type::Boolean:
            return "Boolean";
        case Type::Control:
            return "control";
        case Type::Void:
            return "nothing";
        default:
            return "unknown";
    }
}

ast::Module parseModule(const std::vector<Token> &tokens, Diagnostics &diagnostics)
{
    return Parser(tokens, diagnostics).run();
}

} // namespace tf
