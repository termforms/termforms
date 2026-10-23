// SPDX-License-Identifier: BSD-2-Clause
//
// c_emitter.hpp -- lowers a checked module to C99 source.
//
// The generated C is meant to be read (and stepped through) next to the
// BASIC source:
//   * every BASIC statement becomes one C line, preceded by a #line
//     directive, so gdb, lldb and Visual Studio show and step BASIC lines;
//   * names are mangled predictably: c_<control>, g_<module variable>,
//     v_<local variable>, p_<Sub>, s_<n> for string literals;
//   * string temporaries are released on the same line that creates them.

#pragma once

#include "ast/ast.hpp"

#include <string>

namespace tf
{

struct EmitOptions
{
    std::string sourcePath; // as written in #line directives
    std::string version;    // compiler version, for the header comment
};

// The module must have passed checkModule() without errors.
std::string emitC(const ast::Module &module, const EmitOptions &options);

} // namespace tf
