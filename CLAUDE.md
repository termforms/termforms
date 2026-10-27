# CLAUDE.md — working notes for TermForms

Read this first in every session. It is the handover from the design and
Phase 1–2 work (Oct 2026); keep it current when decisions or status change.

## The project and the person

TermForms is a cross-platform language with VB-DOS-style syntax ("TermForms
BASIC") for event-driven, text-mode forms applications in a terminal, used
locally or over SSH. `tfc` compiles `.bas` to C99, then drives a C compiler.
Primary use: Jim's own projects; secondary: others wanting lightweight
programs on Linux servers.

Jim (repo owner) has 45+ years of commercial development; used Subversion,
new to GitHub workflows. He wants:

- Clarifying questions before starting work that is expensive to redo.
- Professional-level comments in code (the reason, not the mechanics).
- The required environment stated when new tools or libraries come in.
- Direct, honest advice over agreement.

Workflow: commits go **straight to `main`** (Jim chose this over PRs). Run
the full test suite and clang-format before every commit; CI must stay green.

## Reference documents (the fuller story)

- Design handover (decisions, rationale): https://claude.ai/artifact/NEp5GCrSWX11gW6HrYtA1w
- Project plan (phases, gates, risks): https://claude.ai/artifact/AmhbZ25CTKbrz1KUx6Jg79
- Development environment (per platform): https://claude.ai/artifact/7YUAsw8DCV3PFHxGGDq151
- In repo: `docs/REFERENCE.md` (the language as decided: every syntax and
  semantics rule, marked Settled or Proposed; update it with each
  decision), `docs/LANGUAGE.md` (what works today), `docs/CODING_STANDARDS.md`,
  `docs/LICENSING.md`, `toolchain.txt`.

These are Claude Docs; they may not be readable from Claude Code. If this
file and they disagree, this file is newer for status; the handover is
authoritative for design rationale.

## Status (27 Oct 2026, Phase 3 step 4 done)

- **Phase 1 (foundations): done.**
- **Phase 2 (vertical slice): done (26 Oct 2026).** `hello` built on the M4
  for all three targets ran without errors or differences on the Mac, the
  Win11 ThinkPad and a DreamHost Linux server (over SSH, static musl
  binary). Carried into the Phase 4 terminal matrix, not yet confirmed:
  1. Terminal.app (with "Use Option as Meta key") and Ghostty on the Mac.
  2. Dropping the SSH connection mid-run restores the terminal (SIGHUP).
  3. Windows: double-click launch (console host or Windows Terminal?).
  4. Resize behaviour: re-centring, and the "Please resize" overlay.
  5. Can Visual Studio debug zig-built (MinGW ABI, DWARF) programs? Needs
     VS on the ThinkPad. If not, Windows debug builds use MSVC (`--cc cl`).
- **Phase 3 (language core): in progress.** Step 1 done (27 Oct 2026):
  Sub/Function with ByVal/ByRef parameters, Return and name-assignment
  results, the three call syntaxes, For/Do/While loops, Exit, block scope.
  Jim settled (27 Oct 2026) the For semantics, a Dim inside a loop
  resetting each pass, and strict left-to-right evaluation.
  Step 2 done (27 Oct 2026): Long and Double, `/ \ Mod ^`, widening,
  CInt/CLng/CDbl/CStr/Str$/Val/Fix/Int. Jim settled the Double text
  format, Str$'s space, banker's rounding and errors instead of
  Infinity/NaN, then (same day) the remaining number rules in
  `docs/REFERENCE.md`.
  Step 3 done (27 Oct 2026): Try/Catch [When]/Finally, Throw, Err.Raise
  (VB's Number, Source, Description), Err.Clear, Err.Number/Description/
  Source/Line. Jim settled Err.Raise's form and out of memory being fatal;
  the other details are *Proposed* in `docs/REFERENCE.md` ("Errors
  (Proposed)") — confirm them with Jim.
  Error numbers (27 Oct 2026, settled): VB6's number and exact text where
  the meaning matches, never a VB6 number for something else; runtime
  1-899 (VB6) and 900-999 (TermForms-only, e.g. 900 Terminal error);
  programs from 1000. Texts live only in `tf_error_text`; raise sites pass
  NULL. `docs/REFERENCE.md` "Error numbers" lists which numbers each
  coming step adds (9, 10, 91, 457, 52-76, 380). A string too long is 14,
  catchable (checked before allocating); out of memory stays 7, fatal.
  **Const (and Enum unless complex) added to v1 scope** (Jim, 27 Oct
  2026); their detailed rules are still to be settled with him.
  Step 4 done (27 Oct 2026): Const and Enum, CType; all their rules
  settled, including the details. Error details settled too (27 Oct
  2026), including **End inside a Try runs no Finally** (documented in
  REFERENCE.md and LANGUAGE.md), and the rest of the error rules. Nothing
  in REFERENCE.md is Proposed at present.
- **Next:** Phase 3 step 5 (arrays, Types and Ref, Dictionary), or Phase 4
  work; ask Jim.

## Settled design decisions (do not relitigate without Jim)

- **Target:** plain C99. **Compiler:** C++17, standard library only.
  **Runtime:** C99, no third-party dependencies (C library + POSIX/Win32).
- **Platforms:** Linux, macOS (Apple Silicon), Windows x64, equally
  supported. One VT escape-sequence path everywhere; Windows needs
  10 1809+ (conhost and Windows Terminal both supported).
- **Types:** Integer = 32-bit, Long = 64-bit, Double, String (UTF-8,
  refcounted, immutable), Boolean. No Variants. Overflow is a runtime error.
- **Declarations:** `Dim x As T` normal; suffixes `% & # $` optional.
  Option Explicit always on (cannot be disabled); Option Strict on by
  default (widening implicit; narrowing needs CInt/CLng; String↔number
  always explicit, except `&` which converts operands to text).
- **Case:** keywords case-insensitive; *everything else case-sensitive*
  (user identifiers, built-in names, properties, events). Warn when a Sub
  matches an event handler name except for case.
- **Arrays:** 0-based, `Dim a(10)` = 11 elements, no Option Base (document
  prominently). Dynamic arrays with ReDim Preserve, bounds-checked.
- **Strings:** `Len`/`Mid$` etc. count code points; `LenB` returns bytes;
  the renderer measures display width separately (wide chars = 2 cells,
  needs Unicode width tables, see Licensing).
- **Objects:** no user classes. User `Type`s; `Ref Customer` is a reference
  type (starts Nothing); `New` allocates; `Dim c As New Customer` is
  shorthand. Refcounted; cycles leak (accepted). No `Set`. `Is`/`IsNot
  Nothing`. Self-referential Types use Ref members.
- **Errors:** `Try / Catch [When cond] / Finally / End Try`; `Throw num, desc`
  (Err.Raise alias); bare Throw rethrows; built-in `Err` (Number,
  Description, Line). Finally runs on every exit path (Exit Sub/Function,
  Return, Exit For/Do leaving the Try, propagating error) via a
  pending-action variable; error in Finally replaces the pending one. No
  GoTo into/out of Try. Uncaught error in an event handler: restore
  terminal, report file:line, exit. **Lowering: status checks + goto to the
  Catch/cleanup label, not setjmp/longjmp.**
- **Parameters (26 Oct 2026):** ByVal is the default (VB.NET, not VB-DOS);
  `ByRef` marks a parameter the callee may assign to. ByVal/ByRef appear
  only in the signature, never at the call site (as VB6). A ByRef argument
  must be assignable (variable, array element, Type field); anything else
  is a compile error. A user Type passed ByVal is copied whole. An array
  passed ByVal is read-only in the callee (element assignment, ReDim, or
  passing it on ByRef is a compile error) — no copy is made; pass ByRef to
  modify. A `Ref` passed ByVal copies the reference: the callee can change
  the object's fields but not rebind the caller's variable.
- **Procedures and loops (27 Oct 2026):** Functions return by assigning
  to their name or with `Return` (bare name inside = result; `F()` = call).
  Calls: `S a, b`, `S(a, b)`, `Call S(a, b)` all equivalent. While loops:
  `Wend` and `End While` both accepted. `Dim` is block-scoped (VB.NET); no
  hiding an enclosing local. Full rules: `docs/REFERENCE.md`.
- **Events:** `Sub <Control>_<Event>` wired at compile time into dispatch
  (no reflection). `Form_Load` runs before the form shows.
- **Forms:** fixed size per form (Width/Height include border), absolute
  Left/Top inside the border, centred on a backdrop; modal forms stack.
  Form block (`Begin Form … End` with `Property = value`) at the top of the
  form's `.bas`. Later: separate `.frm` included by shared root name or a
  directive; a visual designer rewrites only that block.
- **v1 controls:** Label, TextBox (single-line), CommandButton, CheckBox,
  OptionButton, ListBox, Frame, Timer; menus (F10 + Alt accelerators, Ctrl
  alternatives for every function key); MsgBox, InputBox. ComboBox and
  multi-line TextBox soon after v1 — so the TextBox sits on a general
  edit-buffer model and the renderer needs an overlay (z-order) layer.
- **Library v1:** Dictionary (string-keyed); Dir$, Kill, MkDir, RmDir,
  ChDir, CurDir$; Open/Line Input/Print #/Close; Command$, Environ$, Shell;
  Now, Timer, Date$, Time$. Proper Date type deferred.
- **C interop:** `Declare Function … Lib` maps to C prototypes.
- **Constants (27 Oct 2026):** `Const` and `Enum` are in v1. Const's type
  comes from its value unless As/suffix says otherwise. Enum members are
  always qualified (`Color.Red`); an Enum is a distinct type (widens to
  Integer/Long implicitly, Integer to Enum needs `CType(n, Color)`); as
  text an Enum value is its number. Details in REFERENCE.md.
- **Debugging:** `#line` before every statement (one C line per BASIC
  statement); pretty-printers for gdb, lldb, .natvis. DAP debugger later.
- **Toolchain:** v1 bundles nothing; driver uses `--cc`, `$TFC_CC`, else
  `cc` (Windows: zig cc, else cl). zig pinned (0.16.0 — 0.17.0 has no
  Windows PyPI package yet). Bundling zig revisited after v1. TinyCC dropped.
- **Files:** `.bas` modules/forms, `.frm` later, `.vbp` project file in our
  own format. **Name:** TermForms; the language "TermForms BASIC".
- **Licence:** compiler/tools BSD-2-Clause; `runtime/` 0BSD (compiled
  programs carry no obligation). Unicode width tables: ship the Unicode
  notice in `runtime/THIRD_PARTY_NOTICES` when the tables arrive (see
  `docs/LICENSING.md`). Every source file starts with an SPDX line.

## Repository map

```
compiler/src/
  diag/       Diagnostics: file:line:col messages; stages never print/throw
  lexer/      tokenize(); keyword list (reserved words) lives here
  ast/        syntax tree; sema fills resolved fields in place
  parser/     hand-written recursive descent with per-statement recovery
  sema/       catalog.* = control types/properties/events table (add controls
              here AND in tf_runtime.h); sema.* = names, types, event wiring
  codegen/    c_emitter: C99 with #line; mangling c_ g_ v_ p_ s_<n>
  driver/     command_line (pure, unit-tested), build (runs the C compiler)
runtime/
  include/termforms/tf_runtime.h   public API used by generated code
  src/core/   strings, checked integers, errors, version
  src/tui/    keys (decoder), screen (shadow buffer), input (terminal or
              headless script), forms (controls, render, event loop)
  src/platform/{posix,win32}/      terminal layers; each guards itself with
              #if so tfc can compile every runtime file on every target
tests/
  unit/       doctest (vendored, third_party/doctest)
  golden/     .bas -> expected .c         tests/tools/tf_test.py drives
  errors/     .bas -> expected .err       golden/errors/run; new cases are
  run/        .bas + .keys -> .screen     just new files (re-run cmake)
              (+ .stderr = must fail with that message)
  runtime/    C tests linked against tfrt
examples/hello.bas                         the vertical-slice program
```

Key mechanisms:

- **tfc compiles the runtime sources with every program** (does not link
  the `tfrt` library). That is what makes cross-builds work: the runtime is
  always built for the target. Default runtime path is this source tree
  (`TF_DEFAULT_RUNTIME_DIR`); `--runtime` / `$TERMFORMS_RUNTIME` override.
  The `tfrt` CMake target exists for warnings coverage and runtime tests.
- **String ownership in generated code:** values are borrowed (variables,
  literals, property reads) or owned (+1: concat, conversions, Function
  results). Owned values read once are parked in `t1..tn` and released on
  the same C line; assignment hands owned values over (`tf_str_assign`),
  borrowed ones via `tf_str_ref`. NULL is "". Literals are cached and
  immortal — so a test of string lifetimes must build its strings at run
  time (`"ab" & "c"`), or freeing bugs cannot show.
- **Procedures in C:** ByRef parameters are pointers (`(*v_x)`); a ByVal
  String parameter is retained on entry and released at End Sub (so the
  caller changing the source variable cannot free it). A Function's result
  is `r_<Name>`. Exit Sub/Return `goto done` when there is cleanup, else
  `return`. Block-scoped locals all live for the whole call (C names
  `v_x`, `v_x_2` for a second block's `x`); a Dim inside a loop resets
  them. Loops are `for (;;)`; Exit from an outer loop is `goto exit_<n>`.
  Never emit a C `switch` (Select Case must lower to if/else), or `break`
  for Exit would hit the switch.
- **Evaluation order:** strictly left to right on every C compiler. Each
  Function call in an expression is hoisted into a temporary in order; a
  read that a later call in the same statement could change (module
  variable, ByRef parameter, local passed ByRef in that statement,
  property) is snapshotted first (`StatementScope::pendingCalls`).
  `prepare()` must run on a statement's expressions before `expr()`.
- **Errors in C** (see the c_emitter.cpp and tf_error.c file comments):
  `tf_raise` (catchable) records the error and returns while a Try is
  active anywhere (`try_depth` > 0), else exits at once; `tf_fatal_error`
  always exits (out of memory, terminal). Runtime functions must return a
  harmless value after `tf_raise` (never do the overflowing operation).
  Generated code tests `tf_error_pending` — but only if the program
  contains a Try (`checks_`); otherwise it is identical to before. Labels
  per Try n: `fail_n` (Catch dispatch), `finally_n`, `ffail_n` (error in
  Finally: discard the saved one), `leave_n` records Exit/Return through
  Finally; outside any Try the target is `done`. `settle()` parks a
  fallible value before use; `checkpoint()` releases temps then jumps.
  `mayFail()` decides what can fail. With multiple modules (step 6),
  `checks_` must become whole-program.
- **Const and Enum:** sema folds every Const and Enum member value at
  compile time (`fold()`, with the runtime's checked-arithmetic rules and
  its Double text) and rewrites each use into an `ExprKind::Constant`
  node; codegen writes the value and gives a Const no storage. An Enum
  type is `Type::Enum` plus `enumDef` on the Variable/Expr; sema compares
  through `TypeInfo` (`assignable`, `article`), and `lowered()` / codegen's
  `storage()` turn an Enum into its underlying Integer/Long. `CType(x, E)`
  is checked as CInt/CLng (`checkCType`), so it needs no code of its own.
- **Numbers in C:** Integer `int32_t`, Long `int64_t`, Double `double`,
  Boolean `int`. Arithmetic calls `tf_i32_`/`tf_i64_`/`tf_f64_` + op by the
  *result* type; narrower operands widen through the prototypes. Built-in
  functions (`Expr::builtin`, table `kBuiltins` in sema) are pure, so they
  are not hoisted like Function calls (`countCalls` skips them). Double
  literals are written with the shortest exact digits.
- **Headless mode:** `TERMFORMS_HEADLESS='keys{TAB}{ENTER}'` replaces the
  terminal with a key script and dumps the final screen to stdout;
  `TERMFORMS_HEADLESS_SIZE=60x14`. Runtime errors print
  `file:line: runtime error N: message` to stderr, exit 1, after restoring
  the terminal. This is how CI tests real programs without a terminal.
  The dump draws the form first, so it shows what the program last set
  even when an error or End ends it. On macOS, `leaks --atExit -- prog`
  (with `MallocStackLogging=1`) checks a headless run for leaks; CI's
  Linux sanitizer job does the same with LeakSanitizer.
- **Runtime version** is in `runtime/include/termforms/tf_version.h`
  (header, because tfc compiles the runtime directly); the
  `runtime.version` test fails if it drifts from `project(VERSION)`.
- Golden `.c` files are excluded from clang-format (they are compared
  byte for byte).

## Build, test, run

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
#   presets: dev, release, asan (macOS/Linux, Ninja); vs (Windows, VS generator)
#   CI uses ci-unix / ci-asan / ci-windows (warnings as errors)
TF_UPDATE_EXPECTED=1 ctest --preset dev -L golden   # regenerate, then REVIEW the diff
TFC_CC="zig cc" ctest --preset dev -L run           # run tests with another compiler

cd examples
../build/dev/compiler/tfc hello.bas && ./hello                    # native
../build/dev/compiler/tfc --cc "zig cc -target x86_64-linux-musl" -o hello-linux hello.bas
../build/dev/compiler/tfc --cc "zig cc -target x86_64-windows-gnu" -o hello.exe hello.bas
../build/dev/compiler/tfc --emit-c -o - hello.bas                 # inspect generated C
TERMFORMS_HEADLESS='Jim{TAB}{ENTER}' ./hello                      # headless check
```

Formatting (CI enforces; Allman, 4 spaces, 100 columns):

```sh
git ls-files '*.c' '*.h' '*.cpp' '*.hpp' ':!:third_party/**' ':!:tests/golden/**' \
  | xargs clang-format -i        # clang-format 23.1.3 exactly (pipx/pip)
```

## Jim's machines

- **M4 Mac mini, macOS 27**: main dev machine. Checkout at `~/code/termforms`.
  Uses the Command Line Tools toolchain only (an out-of-date full Xcode
  caused `ld: tapi error ... arm64e.x1`; Xcode was removed). Homebrew cmake,
  ninja, pipx clang-format. The pinned zig 0.16.0 is the pipx `ziglang`
  package, run as `python-zig` (`--cc "python-zig cc -target ..."`); plain
  `zig` on PATH is Homebrew's 0.17.0 and must not be used for builds.
  Terminals: iTerm2 (verified), Terminal.app, Ghostty.
- **Linux server, x86-64**: deployment target over SSH; runs static musl
  binaries, nothing installed.
- **Lenovo ThinkPad, Windows 11 x64**: Windows testing; no dev tools yet.

## CI (`.github/workflows/ci.yml`)

12 jobs: build+test on Ubuntu gcc, Ubuntu clang, macOS clang, Windows MSVC
(run tests use the same compiler via `TFC_CC`; Windows uses
`ilammy/msvc-dev-cmd` so tfc can call `cl`); zig cc run tests on all three
OSes; cross build on macOS → static x86-64 musl binary, uploaded and run on
Ubuntu; ASan+UBSan on Linux (with LeakSanitizer) and macOS — `TF_SANITIZE`
instruments tfc and the runtime tests, and `TFC_CC` with `-fsanitize`
instruments the programs the run tests build; clang-format.
Runner images pinned (ubuntu-24.04, macos-15, windows-2025).

Linking: the driver passes `-lm` to gcc-style compilers (glibc keeps
`pow`/`fmod` in libm; macOS, musl and zig's MinGW accept it). The `tfrt`
CMake target links `m` on UNIX for the same reason.

Lessons from getting CI green: MSVC flags unreachable code after an
`if (constexpr-bool) return` (use `if constexpr … else`); clang flags unused
static functions behind a constexpr branch (`[[maybe_unused]]`); Windows
console Python needs UTF-8 stdout for box-drawing; `tf_platform_getenv`'s
buffer is reused by the next call (copy what you keep).

## Known limitations of the slice (intentional, to be replaced)

- One source file, one form; no arrays, no Types, no
  Select Case, no `For i As Integer = ...` inline declaration, no
  `Dim x As Integer = 5` initialisers.
- Controls: TextBox and CommandButton only; no Label, menus, MsgBox.
- Ctrl+Q closes the form (placeholder until menus/End/close semantics).
- Every code point is one cell (no width tables yet); no mouse yet
  (SGR mouse is planned); no Alt keys (ESC-prefix dropped in tf_keys.c).
- Generated `main()` uses `tf_form_run`, a blocking loop; modal stacks and
  DoEvents come with Phase 4.

## Next work (suggested order — confirm with Jim)

Phase 3, language core:
1. ~~Loops, Exit, Function, parameters, block scope~~ (done 27 Oct 2026).
2. ~~Long and Double, conversions, Option Strict widening~~ (done 27 Oct 2026).
3. ~~Try/Catch/Finally, Err, Throw~~ (done 27 Oct 2026).
4. ~~Const and Enum~~ (done 27 Oct 2026).
5. Arrays (0-based, ReDim Preserve, bounds checks), Types and Ref, Dictionary.
6. String library (Mid$, Left$, InStr, …), file I/O, Command$, Environ$.
7. Multiple modules and the `.vbp` project file.

Phase 4, TUI runtime: Label, CheckBox, OptionButton, ListBox, Frame, Timer;
menus (F10, Alt+letter); MsgBox/InputBox and a modal form stack; overlay
layer; edit-buffer TextBox; SGR mouse; Unicode width tables (+ notice file).

Each step: unit tests for rules/messages, golden C for codegen shape, run
tests for behaviour; update `docs/LANGUAGE.md` and `docs/REFERENCE.md`.
Generated C must compile clean with `-Wall -Wextra -Wpedantic -Werror`
(check the golden `.c` files by hand when codegen changes).
