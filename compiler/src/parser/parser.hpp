// SPDX-License-Identifier: BSD-2-Clause
//
// parser.hpp -- builds the syntax tree from tokens.
//
// Hand-written recursive descent: BASIC's grammar (statement-level keywords,
// optional parentheses, single-line and block If) is easier to get right,
// and to report errors for, by hand than with a generator. After an error the
// parser skips to the end of the statement and carries on, so one build
// reports as many independent problems as it can.

#pragma once

#include "ast/ast.hpp"
#include "lexer/lexer.hpp"

#include <vector>

namespace tf
{

ast::Module parseModule(const std::vector<Token> &tokens, Diagnostics &diagnostics);

} // namespace tf
