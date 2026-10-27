// SPDX-License-Identifier: BSD-2-Clause
//
// sema.hpp -- semantic checks: names, types and event wiring.
//
// Rules enforced here (from the design decisions of 23 Oct 2026):
//   * Option Explicit is always on: every variable is declared with Dim
//     before use, with a type (no Variants).
//   * Option Strict: widening is implicit (Integer -> Long -> Double);
//     narrowing needs CInt/CLng and String/number conversion is always
//     explicit (CStr, Val, CInt...). The & operator is the one place a
//     number becomes text implicitly.
//   * Identifiers are case-sensitive. A Sub that matches a control's event
//     handler except for case gets a warning, because it would silently
//     never be called.
//   * Sub <Control>_<Event> wires the Sub to that event; Form_Load runs
//     before the form is shown.

#pragma once

#include "ast/ast.hpp"

namespace tf
{

// Annotates the module in place. Returns false if errors were reported.
bool checkModule(ast::Module &module, Diagnostics &diagnostics);

} // namespace tf
