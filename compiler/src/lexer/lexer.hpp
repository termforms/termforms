// SPDX-License-Identifier: BSD-2-Clause
//
// lexer.hpp -- turns TermForms BASIC source text into tokens.
//
// Rules worth knowing:
//   * Keywords are case-insensitive and reserved: "dim", "Dim" and "DIM" are
//     all the keyword DIM. Every other identifier is case-sensitive.
//   * An identifier may end in a type suffix (% & # $), which is kept apart
//     from the name: "Name$" is the name "Name" with suffix '$'.
//   * Statements end at a newline or ':'. A line ending in " _" continues on
//     the next line.
//   * Comments start with ' or with the REM keyword and run to the end of
//     the line.

#pragma once

#include "diag/diagnostics.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tf
{

enum class TokenKind
{
    EndOfFile,
    EndOfStatement, // newline or ':'
    Identifier,
    Keyword, // text holds the upper-case keyword, e.g. "DIM"
    IntegerLiteral,
    StringLiteral, // text holds the decoded value
    Symbol         // operators and punctuation; text holds e.g. "<=" or "("
};

struct Token
{
    TokenKind kind = TokenKind::EndOfFile;
    std::string text;
    char suffix = 0;          // type suffix on an identifier, or 0
    std::int64_t integer = 0; // value of an IntegerLiteral
    Location location;
    std::string spelling; // keyword as written, for messages

    bool isKeyword(std::string_view upper) const
    {
        return kind == TokenKind::Keyword && text == upper;
    }
    bool isSymbol(std::string_view symbol) const
    {
        return kind == TokenKind::Symbol && text == symbol;
    }
};

// Tokenises a whole file. Problems are reported to diagnostics; the token
// list always ends with EndOfStatement followed by EndOfFile.
std::vector<Token> tokenize(std::string_view source, Diagnostics &diagnostics);

// True if word (any case) is a reserved keyword.
bool isReservedKeyword(std::string_view word);

} // namespace tf
