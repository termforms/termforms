// SPDX-License-Identifier: BSD-2-Clause
//
// parser.cpp -- see parser.hpp.
//
// Grammar of this version (keywords upper case, EOS = end of statement):
//
//   module     := { formBlock | sub | dim EOS }
//   formBlock  := BEGIN Form name EOS { setting | control } END EOS
//   control    := BEGIN type name EOS { setting } END EOS
//   setting    := name '=' ['-'] literal EOS
//   sub        := SUB name ['(' ')'] EOS block END SUB EOS
//   statement  := dim | if | CALL name ['(' ')'] | END
//               | target '=' expr | name ['(' ')']
//   dim        := DIM name [AS type] { ',' name [AS type] }
//   if         := IF expr THEN EOS block { ELSEIF expr THEN EOS block }
//                 [ELSE EOS block] END IF
//               | IF expr THEN statement [ELSE statement]   (single line)
//   expr       := or-expression, with VB precedence from lowest to highest:
//                 Or, And, Not, comparisons, &, + -, \, * /, unary -

#include "parser/parser.hpp"

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
        module.fileName = diagnostics_.fileName();
        skipStatementEnds();

        while (!at(TokenKind::EndOfFile))
        {
            if (peek().isKeyword("BEGIN"))
            {
                parseFormBlock(module);
            }
            else if (peek().isKeyword("SUB"))
            {
                module.procedures.push_back(parseSub());
            }
            else if (peek().isKeyword("DIM"))
            {
                module.moduleStatements.push_back(parseDim(&module, nullptr));
                expectEndOfStatement();
            }
            else
            {
                error(peek().location, "statements must be inside a Sub");
                skipStatement();
            }
            skipStatementEnds();
        }
        return module;
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
                error(peek().location, "controls cannot contain other controls in this version");
                skipStatement();
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

    std::unique_ptr<Procedure> parseSub()
    {
        auto proc = std::make_unique<Procedure>();
        proc->location = next().location; // SUB
        proc->name = expectIdentifier("a Sub name");
        if (acceptSymbol("("))
        {
            if (!peek().isSymbol(")"))
            {
                error(peek().location, "Sub parameters are not supported yet");
                while (!at(TokenKind::EndOfStatement) && !peek().isSymbol(")"))
                {
                    next();
                }
            }
            expectSymbol(")");
        }
        expectEndOfStatement();

        currentProcedure_ = proc.get();
        proc->body = parseBlock([](const Parser &p) { return p.isEndOf("SUB"); });
        currentProcedure_ = nullptr;

        proc->endLocation = peek().location;
        if (expectKeyword("END", "End Sub"))
        {
            expectKeyword("SUB", "Sub");
        }
        expectEndOfStatement();
        return proc;
    }

    // True at "End <keyword>".
    bool isEndOf(std::string_view keyword) const
    {
        return peek().isKeyword("END") && peek(1).isKeyword(keyword);
    }

    template <typename Stop> Block parseBlock(Stop stop)
    {
        Block block;
        skipStatementEnds();
        while (!at(TokenKind::EndOfFile) && !stop(*this))
        {
            // Stray block ends belong to an outer construct the user got wrong.
            if (peek().isKeyword("SUB") || isEndOf("SUB"))
            {
                break;
            }
            if (StmtPtr s = parseStatement())
            {
                block.push_back(std::move(s));
            }
            skipStatementEnds();
        }
        return block;
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
        if (t.isKeyword("CALL"))
        {
            next();
            StmtPtr s = parseCall(peek().location);
            expectEndOfStatement();
            return s;
        }
        if (t.isKeyword("END") &&
            (peek(1).kind == TokenKind::EndOfStatement || peek(1).kind == TokenKind::EndOfFile))
        {
            auto s = std::make_unique<Stmt>();
            s->kind = StmtKind::End;
            s->location = next().location;
            expectEndOfStatement();
            return s;
        }
        if (t.kind == TokenKind::Identifier)
        {
            // "name = ..." or "name.Prop = ..." is an assignment; a bare name
            // (optionally with "()") is a call.
            if (peek(1).isSymbol("=") || peek(1).isSymbol("."))
            {
                return parseAssignment();
            }
            StmtPtr s = parseCall(t.location);
            expectEndOfStatement();
            return s;
        }

        error(t.location, "expected a statement but found " + describe(t));
        skipStatement();
        return nullptr;
    }

    StmtPtr parseDim(Module *module, Procedure *procedure)
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Dim;
        s->location = next().location; // DIM

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
            pending.push_back(std::move(v));

            if (acceptKeyword("AS"))
            {
                const Location typeAt = peek().location;
                const Type declared = parseTypeName();
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
                    }
                    else if (adjacent && p.type != declared && declared != Type::Error)
                    {
                        error(typeAt, "'" + p.name + "' has a type suffix that contradicts 'As " +
                                          typeName(declared) + "'");
                    }
                }
                flush(pending, *s, module, procedure);
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
                error(p->location, "'" + p->name +
                                       "' needs a type: add 'As Integer', 'As String' "
                                       "or 'As Boolean'");
            }
        }
        flush(pending, *s, module, procedure);
        return s;
    }

    static Type typeFromSuffix(char suffix)
    {
        switch (suffix)
        {
            case '%':
                return Type::Integer;
            case '$':
                return Type::String;
            default:
                return Type::Error;
        }
    }

    Type parseTypeName()
    {
        if (acceptKeyword("INTEGER"))
        {
            return Type::Integer;
        }
        if (acceptKeyword("STRING"))
        {
            return Type::String;
        }
        if (acceptKeyword("BOOLEAN"))
        {
            return Type::Boolean;
        }
        if (peek().isKeyword("LONG") || peek().isKeyword("DOUBLE"))
        {
            error(peek().location, "type '" + peek().spelling + "' is not supported yet");
            next();
            return Type::Error;
        }
        error(peek().location, "expected a type but found " + describe(peek()));
        return Type::Error;
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
            first.body.push_back(parseInlineStatement());
            s->arms.push_back(std::move(first));
            if (acceptKeyword("ELSE"))
            {
                s->hasElse = true;
                s->elseBody.push_back(parseInlineStatement());
            }
            expectEndOfStatement();
            return s;
        }

        auto armEnds = [](const Parser &p) {
            return p.peek().isKeyword("ELSEIF") || p.peek().isKeyword("ELSE") || p.isEndOf("IF");
        };
        first.body = parseBlock(armEnds);
        s->arms.push_back(std::move(first));

        while (peek().isKeyword("ELSEIF"))
        {
            next();
            IfArm arm;
            arm.condition = parseExpression();
            expectKeyword("THEN", "Then");
            expectEndOfStatement();
            arm.body = parseBlock(armEnds);
            s->arms.push_back(std::move(arm));
        }
        if (acceptKeyword("ELSE"))
        {
            s->hasElse = true;
            expectEndOfStatement();
            s->elseBody = parseBlock([](const Parser &p) { return p.isEndOf("IF"); });
        }
        if (expectKeyword("END", "End If"))
        {
            expectKeyword("IF", "If");
        }
        expectEndOfStatement();
        return s;
    }

    // A statement on the same line as Then/Else: no Dim, no block If.
    StmtPtr parseInlineStatement()
    {
        if (peek().isKeyword("END") &&
            (peek(1).kind == TokenKind::EndOfStatement || peek(1).isKeyword("ELSE")))
        {
            auto s = std::make_unique<Stmt>();
            s->kind = StmtKind::End;
            s->location = next().location;
            return s;
        }
        if (peek().isKeyword("CALL"))
        {
            next();
            return parseCall(peek().location);
        }
        if (at(TokenKind::Identifier))
        {
            if (peek(1).isSymbol("=") || peek(1).isSymbol("."))
            {
                return parseAssignmentNoEnd();
            }
            return parseCall(peek().location);
        }
        error(peek().location, "expected a statement after 'Then' or 'Else'");
        return nullptr;
    }

    StmtPtr parseCall(Location where)
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Call;
        s->location = where;
        s->callee = expectIdentifier("a Sub name");
        if (acceptSymbol("("))
        {
            if (!peek().isSymbol(")"))
            {
                error(peek().location, "arguments are not supported yet");
                while (!at(TokenKind::EndOfStatement) && !peek().isSymbol(")"))
                {
                    next();
                }
            }
            expectSymbol(")");
        }
        return s;
    }

    StmtPtr parseAssignment()
    {
        StmtPtr s = parseAssignmentNoEnd();
        expectEndOfStatement();
        return s;
    }

    StmtPtr parseAssignmentNoEnd()
    {
        auto s = std::make_unique<Stmt>();
        s->kind = StmtKind::Assign;
        s->location = peek().location;
        s->target = parsePrimary();
        expectSymbol("=");
        s->value = parseExpression();
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
        ExprPtr left = parseIntegerDivision();
        while (peek().isSymbol("+") || peek().isSymbol("-"))
        {
            const Token &op = next();
            left = makeBinary(op.text, op.location, std::move(left), parseIntegerDivision());
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
        return parsePrimary();
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
                e->integer = next().integer;
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
                if (acceptSymbol("."))
                {
                    auto member = std::make_unique<Expr>();
                    member->kind = ExprKind::Member;
                    member->location = peek().location;
                    member->text = expectIdentifier("a property name");
                    member->left = std::move(e);
                    return member;
                }
                return e;
            }
            default:
                break;
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

        error(t.location, "expected an expression but found " + describe(t));
        e->kind = ExprKind::IntegerLiteral; // placeholder so later stages have a node
        e->type = Type::Error;
        return e;
    }

    const std::vector<Token> &tokens_;
    Diagnostics &diagnostics_;
    std::size_t pos_ = 0;
    int lastErrorLine_ = 0;
    Procedure *currentProcedure_ = nullptr;
};

} // namespace

const char *ast::typeName(Type type)
{
    switch (type)
    {
        case Type::Integer:
            return "Integer";
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
