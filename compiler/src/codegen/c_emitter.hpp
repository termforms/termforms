// SPDX-License-Identifier: BSD-2-Clause
//
// c_emitter.hpp -- lowers a checked module to C99 source.
//
// The generated C is meant to be read (and stepped through) next to the
// BASIC source:
//   * every BASIC statement becomes one C line, preceded by a #line
//     directive, so gdb, lldb and Visual Studio show and step BASIC lines;
//   * names are mangled predictably: c_<control>, g_<module variable>,
//     v_<local variable or parameter> (v_<name>_2 for a second block-scoped
//     variable of the same name), r_<Function> for a Function's result,
//     p_<Sub or Function>, s_<n> for string literals;
//   * string temporaries are released on the same line that creates them.

#pragma once

#include "ast/ast.hpp"

#include <string>
#include <vector>

namespace tf
{

struct EmitOptions
{
    std::string sourcePath; // as written in #line directives
    // With several modules: each file's path, indexed as Location::file.
    std::vector<std::string> sourcePaths;
    std::string version; // compiler version, for the header comment
};

// The module must have passed checkModule() without errors.
std::string emitC(const ast::Module &module, const EmitOptions &options);

// The whole program, its modules in order, as one C file.
std::string emitC(const std::vector<const ast::Module *> &modules, const EmitOptions &options);

} // namespace tf
