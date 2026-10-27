// SPDX-License-Identifier: BSD-2-Clause
//
// lexer.cpp -- see lexer.hpp.

#include "lexer/lexer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace tf
{

namespace
{

// Reserved words, upper case. Some are not used by the parser yet but are
// reserved now so programs written today keep compiling as the language grows.
constexpr std::array<std::string_view, 41> kKeywords = {
    "AND",    "AS",       "BEGIN",  "BOOLEAN", "BYREF",  "BYVAL", "CALL",   "CASE",  "CATCH",
    "DIM",    "DO",       "DOUBLE", "ELSE",    "ELSEIF", "END",   "EXIT",   "FALSE", "FINALLY",
    "FOR",    "FUNCTION", "IF",     "INTEGER", "LONG",   "LOOP",  "MOD",    "NEXT",  "NOT",
    "OPTION", "OR",       "REM",    "RETURN",  "SELECT", "STEP",  "STRING", "SUB",   "THEN",
    "TO",     "TRUE",     "UNTIL",  "WEND",    "WHILE"};

std::string toUpper(std::string_view text)
{
    std::string upper(text);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return upper;
}

bool isIdentStart(char c)
{
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
}

bool isIdentPart(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool isSuffix(char c)
{
    return c == '%' || c == '&' || c == '#' || c == '$';
}

class Lexer
{
public:
    Lexer(std::string_view source, Diagnostics &diagnostics)
        : source_(source), diagnostics_(diagnostics)
    {
    }

    std::vector<Token> run()
    {
        while (!atEnd())
        {
            scanToken();
        }
        endStatement(here());
        Token eof;
        eof.kind = TokenKind::EndOfFile;
        eof.location = here();
        tokens_.push_back(eof);
        return std::move(tokens_);
    }

private:
    bool atEnd() const { return pos_ >= source_.size(); }
    char peek(std::size_t ahead = 0) const
    {
        return pos_ + ahead < source_.size() ? source_[pos_ + ahead] : '\0';
    }
    Location here() const { return {line_, column_}; }

    void advance()
    {
        if (source_[pos_] == '\n')
        {
            ++line_;
            column_ = 1;
        }
        else
        {
            ++column_;
        }
        ++pos_;
    }

    void skipToEndOfLine()
    {
        while (!atEnd() && peek() != '\n')
        {
            advance();
        }
    }

    // Collapses runs of statement ends, and never starts the file with one,
    // so the parser sees each separator once.
    void endStatement(Location where)
    {
        if (tokens_.empty() || tokens_.back().kind == TokenKind::EndOfStatement)
        {
            return;
        }
        Token t;
        t.kind = TokenKind::EndOfStatement;
        t.location = where;
        tokens_.push_back(t);
    }

    void scanToken()
    {
        const char c = peek();
        const Location start = here();

        if (c == ' ' || c == '\t' || c == '\r')
        {
            advance();
            return;
        }
        if (c == '\n' || c == ':')
        {
            endStatement(start);
            advance();
            return;
        }
        if (c == '\'')
        {
            skipToEndOfLine();
            return;
        }
        if (c == '_' && !isIdentPart(peek(1)))
        {
            continuation(start);
            return;
        }
        if (isIdentStart(c))
        {
            identifier(start);
            return;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) != 0)
        {
            number(start);
            return;
        }
        if (c == '"')
        {
            string(start);
            return;
        }
        if (c == '&' && (peek(1) == 'H' || peek(1) == 'h' || peek(1) == 'O' || peek(1) == 'o') &&
            isDigitIn(peek(2), peek(1) == 'H' || peek(1) == 'h' ? 16 : 8))
        {
            radixNumber(start);
            return;
        }
        symbol(start);
    }

    // " _" at the end of a line joins it to the next one.
    void continuation(Location start)
    {
        advance(); // '_'
        while (peek() == ' ' || peek() == '\t' || peek() == '\r')
        {
            advance();
        }
        if (peek() == '\n')
        {
            advance(); // swallow the newline: the statement goes on
            return;
        }
        if (atEnd())
        {
            return;
        }
        diagnostics_.error(start, "'_' continues a line only at the end of the line");
    }

    void identifier(Location start)
    {
        const std::size_t begin = pos_;
        while (isIdentPart(peek()))
        {
            advance();
        }
        Token t;
        t.location = start;
        t.text = std::string(source_.substr(begin, pos_ - begin));
        if (isSuffix(peek()))
        {
            t.suffix = peek();
            advance();
        }

        const std::string upper = toUpper(t.text);
        if (t.suffix == 0 && isReservedKeyword(upper))
        {
            if (upper == "REM")
            {
                skipToEndOfLine();
                return;
            }
            t.kind = TokenKind::Keyword;
            t.spelling = t.text;
            t.text = upper;
        }
        else
        {
            t.kind = TokenKind::Identifier;
        }
        tokens_.push_back(std::move(t));
    }

    void number(Location start)
    {
        const std::size_t begin = pos_;
        Token t;
        t.kind = TokenKind::IntegerLiteral;
        t.location = start;
        t.integer = digits(10, start);
        bool fraction = false;
        if (peek() == '.' && isDigit(peek(1)))
        {
            fraction = true;
            advance();
            while (isDigit(peek()))
            {
                advance();
            }
        }
        // An exponent counts only when digits follow it.
        if ((peek() == 'E' || peek() == 'e') &&
            (isDigit(peek(1)) || ((peek(1) == '+' || peek(1) == '-') && isDigit(peek(2)))))
        {
            fraction = true;
            advance();
            if (peek() == '+' || peek() == '-')
            {
                advance();
            }
            while (isDigit(peek()))
            {
                advance();
            }
        }
        t.text = std::string(source_.substr(begin, pos_ - begin));
        if (fraction)
        {
            // The compiler runs in the "C" locale, so '.' is the separator.
            t.kind = TokenKind::DoubleLiteral;
            t.real = std::strtod(t.text.c_str(), nullptr);
            if (!std::isfinite(t.real))
            {
                diagnostics_.error(start, "number too large for a Double");
            }
        }
        numberSuffix(t, start);
        tokens_.push_back(std::move(t));
    }

    // &H1F and &O17: hexadecimal and octal Integer or Long literals.
    void radixNumber(Location start)
    {
        advance(); // '&'
        const char letter = peek();
        advance();
        Token t;
        t.kind = TokenKind::IntegerLiteral;
        t.location = start;
        t.text = std::string("&") + letter;
        if (!isDigitIn(peek(), letter == 'H' || letter == 'h' ? 16 : 8))
        {
            diagnostics_.error(start, std::string("expected digits after '&") + letter + "'");
        }
        t.integer = digits(letter == 'H' || letter == 'h' ? 16 : 8, start);
        numberSuffix(t, start);
        tokens_.push_back(std::move(t));
    }

    // Reads digits in the given radix; values beyond a Long are an error.
    std::int64_t digits(int radix, Location start)
    {
        std::int64_t value = 0;
        bool tooBig = false;
        while (isDigitIn(peek(), radix))
        {
            const int digit = digitValue(peek());
            if (value > (INT64_MAX - digit) / radix)
            {
                tooBig = true;
            }
            else
            {
                value = value * radix + digit;
            }
            advance();
        }
        if (tooBig)
        {
            diagnostics_.error(start, "number too large");
        }
        return value;
    }

    // A type suffix written on a number: 5% (Integer), 5& (Long), 5# (Double).
    void numberSuffix(Token &t, Location start)
    {
        const char c = peek();
        if (c != '%' && c != '&' && c != '#')
        {
            return;
        }
        // "5 & x" is concatenation; "5&" directly followed by a name or
        // a quote would be ambiguous, and VB reads it as a Long too.
        advance();
        if (t.kind == TokenKind::DoubleLiteral && c != '#')
        {
            diagnostics_.error(start, std::string("a number with a fraction cannot have the "
                                                  "suffix '") +
                                          c + "'");
            return;
        }
        if (c == '#' && t.kind == TokenKind::IntegerLiteral)
        {
            t.kind = TokenKind::DoubleLiteral;
            t.real = static_cast<double>(t.integer);
            return;
        }
        t.suffix = c;
    }

    static bool isDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }

    static bool isDigitIn(char c, int radix)
    {
        if (radix == 16)
        {
            return std::isxdigit(static_cast<unsigned char>(c)) != 0;
        }
        return c >= '0' && c < '0' + radix;
    }

    static int digitValue(char c)
    {
        return isDigit(c) ? c - '0' : std::tolower(static_cast<unsigned char>(c)) - 'a' + 10;
    }

    // "..." with "" standing for one quote; strings end on the same line.
    void string(Location start)
    {
        Token t;
        t.kind = TokenKind::StringLiteral;
        t.location = start;
        advance(); // opening quote
        for (;;)
        {
            if (atEnd() || peek() == '\n')
            {
                diagnostics_.error(start, "string is missing its closing quote");
                break;
            }
            if (peek() == '"')
            {
                advance();
                if (peek() == '"')
                {
                    t.text += '"';
                    advance();
                    continue;
                }
                break;
            }
            t.text += peek();
            advance();
        }
        tokens_.push_back(std::move(t));
    }

    void symbol(Location start)
    {
        static constexpr std::array<std::string_view, 3> kTwoChar = {"<>", "<=", ">="};
        Token t;
        t.kind = TokenKind::Symbol;
        t.location = start;

        for (std::string_view two : kTwoChar)
        {
            if (peek() == two[0] && peek(1) == two[1])
            {
                t.text = std::string(two);
                advance();
                advance();
                tokens_.push_back(std::move(t));
                return;
            }
        }

        const char c = peek();
        static constexpr std::string_view kSingle = "&+-*/\\^=<>().,";
        if (kSingle.find(c) == std::string_view::npos)
        {
            diagnostics_.error(start, std::string("unexpected character '") + c + "'");
            advance();
            return;
        }
        t.text = std::string(1, c);
        advance();
        tokens_.push_back(std::move(t));
    }

    std::string_view source_;
    Diagnostics &diagnostics_;
    std::vector<Token> tokens_;
    std::size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
};

} // namespace

bool isReservedKeyword(std::string_view word)
{
    const std::string upper = toUpper(word);
    return std::find(kKeywords.begin(), kKeywords.end(), upper) != kKeywords.end();
}

std::vector<Token> tokenize(std::string_view source, Diagnostics &diagnostics)
{
    return Lexer(source, diagnostics).run();
}

} // namespace tf
