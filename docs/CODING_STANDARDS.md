# Coding standards

These apply to the compiler (C++17), the runtime (C99) and, where it makes
sense, to the C the compiler generates. Formatting is not a matter of taste
here: `clang-format` decides it, and CI rejects anything it would change.

## Formatting

- Run `clang-format -i` on changed files before committing. The pinned version
  is in `toolchain.txt` (`pip install clang-format==23.1.3`); other versions
  disagree on details.
- Allman braces, 4-space indent, no tabs, 100-column limit (see `.clang-format`).
- Files are UTF-8 with LF line endings (`.editorconfig`, `.gitattributes`).

## Licence headers

Every source file starts with an SPDX line:

| Location | Header |
| --- | --- |
| `runtime/`, `tests/runtime/` | `SPDX-License-Identifier: 0BSD` |
| everything else | `SPDX-License-Identifier: BSD-2-Clause` |

Runtime code must never include or copy code under a licence that would bind
compiled programs.

## Naming

| Kind | C++ (compiler) | C (runtime) |
| --- | --- | --- |
| Namespaces | `tf`, `tf::driver`, lower case | — |
| Types | `PascalCase` | `tf_snake_case` (typedef'd structs) |
| Functions | `camelCase` | `tf_snake_case` |
| Variables, parameters | `camelCase` | `snake_case` |
| Constants | `kPascalCase` | `TF_UPPER_CASE` (macros and enum values) |
| Files | `snake_case.cpp/.hpp` | `tf_snake_case.c/.h` |

Every public runtime identifier carries the `tf_`/`TF_` prefix, so generated
code and C libraries a program declares cannot collide with it.

## Language rules

- **C++:** standard library only. No exceptions crossing component boundaries:
  compiler diagnostics are reported through return values and diagnostic
  lists, not thrown. No RTTI-dependent designs.
- **C runtime:** strict C99, no compiler extensions without a portable
  fallback, no global mutable state that is not documented as such, and no
  dependency beyond the C library plus POSIX or Win32 (behind `platform/`).
- Warnings are errors in CI on all three platforms (`TF_WARNINGS_AS_ERRORS`).

## Comments

Comment the reason, not the mechanics: why a rule exists, what a function
guarantees, what the caller must ensure. Each file opens with a short
statement of its purpose. Public functions document ownership (who frees what)
and error behaviour.

## Tests

- New compiler behaviour gets doctest unit tests in `tests/unit/`.
- New runtime behaviour gets a C test in `tests/runtime/` (exit 0 = pass).
- Command-line behaviour is tested through CTest in `tests/CMakeLists.txt`.
- Phase 2 adds golden tests (`.bas` to expected C) and run tests (`.bas` to
  expected output).
