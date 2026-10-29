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
Run the format check over *all* files (the `--dry-run -Werror` line below)
last, after the final edit: formatting only the files changed misses a
file edited after that step.

## Reference documents (the fuller story)

- Design handover (decisions, rationale): https://claude.ai/artifact/NEp5GCrSWX11gW6HrYtA1w
- Project plan (phases, gates, risks): https://claude.ai/artifact/AmhbZ25CTKbrz1KUx6Jg79
- Development environment (per platform): https://claude.ai/artifact/7YUAsw8DCV3PFHxGGDq151
- In repo: `docs/GUIDE.md` (the programmer's guide), `docs/REFERENCE.md` (the language as decided: every syntax and
  semantics rule, marked Settled or Proposed; update it with each
  decision), `docs/LANGUAGE.md` (what works today), `docs/CODING_STANDARDS.md`,
  `docs/LICENSING.md`, `toolchain.txt`.

These are Claude Docs; they may not be readable from Claude Code. If this
file and they disagree, this file is newer for status; the handover is
authoritative for design rationale.

## Status (28 Oct 2026, Phase 3 steps 1-6 done)

- **Phase 1 (foundations): done.**
- **Phase 2 (vertical slice): done (26 Oct 2026).** `hello` built on the M4
  for all three targets ran without errors or differences on the Mac, the
  Win11 ThinkPad and a DreamHost Linux server (over SSH, static musl
  binary). Carried into the Phase 4 terminal matrix, not yet confirmed:
  1. ~~Terminal.app and Ghostty on the Mac~~: done (Jim, 28 Oct 2026,
     with the demo): iTerm2, Ghostty and Terminal.app all work once the
     Option key sends Esc (iTerm2: Profiles > Keys, Left Option key
     Esc+; Terminal.app: Profiles > Keyboard, Use Option as Meta key;
     Ghostty: Ghostty > Settings… opens its config in TextEdit, add
     `macos-option-as-alt = left`, then reload). Out of the box all three
     type Option characters (˜ ¥), so no access key arrives. On Jim's
     non-Apple keyboard it is the key labelled "opt" that works; Ctrl
     and Alt do not give access keys (Ctrl+letter is not meant to).
  2. ~~Dropping the SSH connection mid-run restores the terminal
     (SIGHUP)~~: done (Jim, 28 Oct 2026): the demo's static musl build
     on the Linux server over SSH works as expected, mouse and menus
     included.
  3. ~~Windows: double-click launch~~: done (Jim, 28 Oct 2026, on his
     wife's laptop, an older ThinkPad with Windows 11 and only Office
     365): double-clicking the demo opens Windows Terminal, and
     everything worked there and in conhost started from the command line.
  4. ~~Resize behaviour: re-centring, and the "Please resize"
     overlay~~: done on the Mac, all three terminals, and on Windows
     (Windows Terminal and conhost) (Jim, 28 Oct 2026).
  5. Can Visual Studio debug zig-built (MinGW ABI, DWARF) programs? Needs
     VS on the ThinkPad. If not, Windows debug builds use MSVC (`--cc cl`).
  6. ~~The mouse on Windows~~: done (Jim, 28 Oct 2026, with the demo,
     in Windows Terminal and conhost, Quick Edit turned off by the
     program): clicks, drags, double-clicks, the wheel, menus and
     bracketed paste all as expected. Originally: build a test app that logs
     every mouse action (click, release elsewhere, drag in a TextBox,
     double-click, wheel, menus) and run it on the ThinkPad in both conhost
     and Windows Terminal, to see that SGR reports arrive through VT input
     and are handled. Cross-built on the Mac (`--cc "python-zig cc -target
     x86_64-windows-gnu"`), so the ThinkPad needs no tools. Also check
     bracketed paste there. Do it with the rest of this matrix.
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
  Step 5a done (27 Oct 2026): arrays (multi-dimensional, 0-based, fixed vs
  dynamic, ReDim [Preserve], Erase, UBound/LBound, copies on assignment,
  array parameters, element ByRef locks its array).
  Step 5b-1 done (27 Oct 2026): Types (value records; arrays of and in
  Types; Type parameters, ByVal copied; Functions returning Types). Jim
  settled: values and Refs convert by copying, arrays in Types, Type
  results, With in v1.
  Step 5b-2 done (27 Oct 2026): Ref, New, Dim As New, Nothing, Is/IsNot,
  value<->Ref copying, Ref members and arrays of Refs, error 91; keywords
  allowed as member names. Also fixed `Return` of a Type (was a shallow
  copy). Smaller Ref/Type rules are Proposed in REFERENCE.md.
  Step 5b-3 done (27 Oct 2026): With ... End With (Types, elements, Refs,
  Function results, controls; nested); `With` reserved; a token's
  `spaced` flag tells `Bump .Age` from `Bump.Age`. Its rules are Proposed
  in REFERENCE.md, except one Jim settled: an element is looked up again
  at each use rather than its array locked as VB6 does (the difference
  only shows when a ReDim inside the With removes the element).
  Step 5c (27 Oct 2026): Jim settled the Dictionary design (REFERENCE.md
  "Dictionary") and For Each in v1. 5c-1 done: For Each over arrays
  (`Each`, `In` reserved).
  5c-2 done: Dictionary (`Of` reserved). Smaller rules Proposed in
  REFERENCE.md "Dictionary: details". Also: an array member or d.Keys
  used as a single value is now an error (`checkWhole`; it used to reach
  the C compiler), and generated Type helpers are `TF_MAYBE_UNUSED`.
  tests/golden/errors.c fails -Wunused-but-set-variable under strict
  flags (its BASIC sets r and never reads it): the test's fault, not
  codegen's; left as is.
  Driver (Jim, 27 Oct 2026): the programmer never sees C compiler output.
  Builds pass `-w` / `/w`; the compiler's messages go to `<out>.tf-log`
  (shown live with `-v`); "cannot run the C compiler" when its program
  is not found (`canRun`, checked before running: cmd.exe's not-found
  status is not dependable); any other C failure is reported as a tfc
  internal error, keeping `.tf.c` and `.tf-log`. Changed (Jim, 29 Oct
  2026, after a mistyped --cc on the laptop): a failure shows the log's
  first 5 lines, and with --cc or TFC_CC it is "error" and says to check
  that command (still offering the report).
- **Step 6 (27 Oct 2026), Jim settled:** string positions 1-based (VB);
  v1 adds Replace, Split/Join, InStrRev, StrReverse; `Print #` simple
  (`;` joins with no spaces, trailing `;` = no newline, no comma zones);
  files written with LF everywhere. Built as 6a (strings), 6b (files,
  Dir$ and the rest).
  6b decisions (Jim, 28 Oct 2026): a Byte type and Byte arrays for binary
  data; binary offsets from 0; Get reads the buffer's size, or an
  optional count; v1 adds Name/FileCopy/FileLen/FileDateTime/Exists, not
  Input #/Write #. 6b-1 done: Byte (`uint8_t`, TF_ELEM_U8; arithmetic is
  Integer via `arithmeticType`; constants 0-255 assign directly through
  `TypeInfo::byteConstant`; CByte; `tf_u8_add` for For steps).
  6b-2 done: text files (tf_file.c; platform tf_fs_<os>.c for UTF-8 paths
  and 64-bit offsets). File words are recognised by shape, not reserved
  (`parseFileStatement`); statements become `StmtKind::Library` calls of
  built-ins in `kStringFunctions`; Line Input is an assignment. Run tests
  now run in `<work>/<name>.dir`, emptied first, so they may write files.
  6b-3 done: binary files (Get/Put/Seek #; the parser passes a 0/1 flag
  before each optional part, so no value means "missing"; library
  parameter kinds L = Long, B = Byte array).
  6b-4 done: the file system (tf_fsys.c on platform calls in tf_fs_<os>.c;
  statements such as Kill parse as Sub calls and become library
  statements in checkCallStatement when no Sub has the name; Name ... As
  by shape; Dir$ sorted, files only).
  6b-5 done: Command$ (main takes argc/argv only in programs that use
  it), Environ$, Shell (waits; exit code; the forms layer's terminal
  hooks hand the terminal back meanwhile), Now, Date$, Time$, Timer.
  **Step 6 is complete.**
  6a done: the string functions (table `kStringFunctions` in ast.hpp,
  shared by sema and codegen; sema fills in defaulted arguments; runtime
  tf_text.c). Jim settled errors 5/14, exact comparison, ASCII-only case
  for now; the other small rules are Proposed in REFERENCE.md "String
  functions". The Mid$ statement, deferred then, was built in 5.2.
  **Binary files soon (Jim, 27 Oct 2026):** read a buffer of x bytes at
  offset y, the position advancing by x on each read, the size or position
  settable in the read or by a separate statement: done in 6b-3.
  Step 5 (arrays, Types, Ref, With, For Each, Dictionary) is complete.
- **Step 7 (28 Oct 2026), Jim settled:** `.vbp` as VB6-style Key=Value
  lines; VB's visibility defaults; one form + modules now (several forms
  in Phase 4); `tfc a.bas b.bas` works without a project file.
  7a done: several modules. Sema: each module registers into its own
  `Names`; `enter(i)` rebuilds the familiar maps (`globals_`, ...) as
  module i's view (own + others' Public; a name Public in two others is
  left out and recorded in `ambiguous_`); `moduleNamed`/`resolveQualified`
  for Module.Name; forms are program-wide (frmMain.ctl). Codegen: one C
  file, `gName/pName/tName` prefix a name with its module only when two
  modules share it (single-file C is unchanged); `TF_AT(f_i, n)` marks
  statements when there are several modules. Tests: a `.with` file lists
  a test's other modules (in a sub-folder the globs skip).
  7b done: the `.vbp` (driver/project.cpp, pure and unit-tested;
  build.cpp resolves its files, then checks Form=/Module=/Startup=
  against the program). A `.vbp` in tests/run or tests/errors is a test.
  **Phase 3 (language core) is complete.**
- **Phase 4 (TUI runtime), decisions settled with Jim 28 Oct 2026**
  (REFERENCE.md "Phase 4: controls, forms and keys"): order Label,
  CheckBox, OptionButton, Frame; ListBox (multi-select too); Timer;
  MsgBox/InputBox + modal stack + vb constants; several forms (a stack);
  menus; edit buffer; mouse (on by default); Unicode widths (from Python's
  unicodedata). Frames contain controls (relative positions, own option
  group). Keys: Default/Cancel buttons, arrows in groups, Alt access
  keys, Space toggles; Ctrl+F4 (Alt+F4) closes the top form; Ctrl+Q
  retires. Events: Click, DblClick, Change, Got/LostFocus, KeyPress,
  KeyDown, Form_Unload(Cancel), Form_Activate.
  **Working mode (Jim, 28 Oct 2026, overnight):** proceed through Phase 4
  unattended, pushing each step once local checks pass and CI goes green;
  stop and wait at anything that needs Jim's decision.
  Step 4.1 done (28 Oct 2026): Label, CheckBox, OptionButton, Frame
  (controls nest in the form block; `tf_control_container`; draw by
  container areas), Value/Default/Cancel/Enabled/Visible, Tab stopping
  once per option group, arrows within it, Space, Enter/Esc to the
  Default/Cancel buttons, Alt access keys, Ctrl+F4/Alt+F4 closing. The
  key decoder now gives Alt+key (ESC prefix), F1-F12 and modifiers; key
  scripts take {ALT+x}, {F4}, {CTRL+F4}.
  Step 4.2 done: ListBox (items in the runtime's tf_control; methods and
  List(i)/Selected(i) are library routines with a ListBox parameter, kind
  C; read-only properties flagged in the catalog; PgUp/PgDn decoded).
  Step 4.3 done: Timer (the form loop waits with tf_input_wait until the
  next due Timer; tf_platform_monotonic_ms; key scripts' {WAIT n} moves a
  virtual clock, so headless Timer tests are exact).
  Step 4.4 done: the form stack (`stack`/`depth` in tf_forms.c; draw_stack;
  run_until_closed is the one event loop, for the main form and dialogs);
  MsgBox and InputBox built at run time from controls (build_dialog, ask);
  VB's named constants (`kNamedConstants` in sema, resolved after
  variables, names reserved). The headless dump happens when the script
  ends, so a test can show a dialog.
  Step 4.5 done: several forms (one per module; tf_form_show/hide/load/
  unload; run_loop runs while forms are shown, or until a modal one
  leaves the stack; Activate then GotFocus on coming to the top), Me,
  startup (Startup=, else Sub Main, else the first form), Form_Load as an
  event the runtime fires on first show, events with arguments
  (catalog Event::params; handlers that may change their argument must
  take it ByRef; tf_control_on_ref / _on_key, no casts), KeyPress and
  KeyDown with VB's codes (vbKey* constants), control C names given the
  form when two forms share one.
  Step 4.6 done: menus (TF_CTL_MENU controls in the form's children, a
  bar menu's container the form, an item's its menu; the form's menu_on/
  menu_top/menu_down/menu_item state; `menu_first` takes the bar key,
  menu access keys and Shortcuts before the focused control; the bar is
  the form's first inside row and the client area starts below it; the
  drop-down drawn last in draw_one). Ctrl+B is F10's alternative
  (Proposed, for Jim). Shortcut text is checked in sema
  (`shortcutProblem`) and read in the runtime (`parse_shortcut`): keep the
  two alike. A ListIndex or Shortcut store is fallible (storeMayFail).
  Step 4.7 done: the TextBox edit buffer (`tf_edit.c`, a `tf_edit` in
  each TextBox: caret and anchor as byte offsets; selection, words, undo
  of a typing run, the program's own clipboard), SelStart/SelLength/
  SelText/MaxLength/Locked/PasswordChar, Change on code changes too (so a
  Text store is fallible), arrows and Home/End/Del decoded with
  modifiers ({SHIFT+LEFT} in key scripts), reads bounded by the
  decoder's room (a long paste was cut at 64 bytes).
  Step 4.8 done: the mouse (SGR 1002+1006, switched on in ui_open and
  ui_resume unless TERMFORMS_MOUSE=0, off in each terminal layer's
  LEAVE_SEQ; TF_KEY_MOUSE with x, y; drawing records each control's
  place (`drawn`, `at_x`...) and `control_at` hit-tests, menus first;
  act on release over the pressed control; key scripts {CLICK x,y} etc.;
  tests/runtime/test_keys.c covers the decoder's terminal bytes).
  Step 4.9 done: Unicode widths (tools/gen_width_tables.py writes
  runtime/src/tui/tf_width_tables.c from Python's unicodedata, Unicode
  17.0 with Python 3.15; tf_char_width/tf_text_cells in tf_width.c; the
  screen keeps a wide character's second cell as TF_CELL_TAIL; captions,
  wrapping and the TextBox measure in cells). **Phase 4 is complete**
  apart from the terminal matrix checks (Phase 2's list above).
  **Phase 4 rules settled (Jim, 28 Oct 2026):** every Phase 4 Proposed
  section in REFERENCE.md approved as built (Ctrl+B for the menu bar,
  Change on code changes, the program's own clipboard, ByRef handler
  arguments, Top 0 below the menu bar), plus two changes, made: bracketed
  paste (tf_keys.c `pasting`; mode 2004 on with the mouse modes, off in
  LEAVE_SEQ) and an unloaded form made afresh at its next Load
  (`tf_form_ready`, emitted after each form's settings, keeps a
  `tf_design` per control; `stale` set by Unload).
- **Language gaps settled (Jim, 28 Oct 2026; REFERENCE.md "Select
  Case", "Declarations with values, and in For", "DoEvents"):** VB6's
  full Select Case (any expressions, first match wins, a warning for an
  unreachable constant Case, matching as `=`); VB.NET's Dim initialisers
  with As (module level: any expression, run at start in module order),
  array initialisers in braces, `For i As Integer` and `For Each w As
  String`; DoEvents as VB (redraw, handle what is waiting, re-entry
  allowed, statement only; closing a form during it lets the code go on).
  Select Case done (28 Oct 2026): `StmtKind::Select` with `cases`
  (CaseClause/CaseTest); sema `checkSelect` checks each test as the
  comparison `_select = v` (so = rules apply) and lowers into `body`: the
  selector into a hidden local, constant Cases as one If/ElseIf chain,
  others through a hidden `_matched` flag; codegen just emits body.
  Boolean And/Or now cast their operands to int (clang's
  -Wbitwise-instead-of-logical, under the strict flags).
  Dim initialisers done: the parser lowers each into statements in the
  Dim's `body` (x = v; a list: ReDim then each element), which sema checks
  as ordinary assignments and codegen emits after the Dim; module-level
  ones are gathered into a hidden Private Sub `_init` per module
  (`Module::initialiser`, first in its procedures) that main calls after
  the forms and event wiring. `{` `}` are now symbols.
  Inline For declarations done: `Stmt::loopVariable` (a procedure local
  the parser makes for `For i As T`), declared by sema in a scope of its
  own around the For.
  DoEvents done: `tf_do_events` (tf_forms.c) shares the event loop's
  `come_to_top` and `handle`; it returns when tf_input_wait(0) has
  nothing (a key script's `{IDLE}`), and a script running out inside it
  ends the program. Form.Visible readable (in_stack). **The four
  settled language gaps are all built.** Generated C under the strict
  flags: a parameter the body never mentions gets `(void)v_x;`
  (`markUnusedParameters`: a handler must take Cancel even unused), and a
  procedure nothing calls gets a `TF_MAYBE_UNUSED` prototype
  (`uncalled_`). A local set but never read still warns; that one is the
  program's (Jim accepted it, see errors.c above).
- **Phase 5 (tooling and docs), scope settled with Jim 28 Oct 2026:**
  build `Declare Function … Lib` (in the v1 table, never built; its
  design gets its own questions first); lldb and gdb: check stepping
  through `.bas` lines, add String/array/Type printers (.natvis after the
  ThinkPad shows whether VS can debug zig builds); a programmer's guide
  in the repo, `docs/GUIDE.md` (REFERENCE.md stays the decision record);
  examples: an every-control demo (also the Phase 4 gate demo and the
  Windows mouse test app), an address book, a file viewer, small
  language samples; CI-built release archives per platform (tfc +
  runtime, tfc finding the runtime next to itself) as GitHub Release
  assets; the Mid$ statement. The Phase 4 gate comes first (demo, then
  the terminal checks); the Phase 5 gate is a clean Windows machine set
  up from the environment doc alone: Jim's wife's laptop (Windows 11,
  only Office 365), as Jim's own ThinkPad has had other development work
  on it. (The Windows terminal checks and the mouse test are already
  done on that laptop.)
  Order (proposed): 5.1 every-control demo, then Jim runs the Mac
  terminal checks with it; 5.2 Mid$ statement; 5.3 Declare (questions
  first); 5.4 lldb/gdb, with VS Code (below); 5.5 the other examples;
  5.6 release archives; 5.7 GUIDE.md; 5.8 the clean-machine gate on
  the laptop.
  **VS Code (Jim, 28 Oct 2026), staged.** With 5.4: a VS Code extension
  (TextMate grammar for highlighting; a build task running tfc with a
  problem matcher for its `file:line:col:` messages) and a CodeLLDB
  `launch.json` template (breakpoints in .bas files by `#line`, the
  program in its own terminal); test the printers in VS Code as well as
  on the command line. Post-v1: a language server `tfls` (diagnostics as
  you type, completion from names and the control catalog, hover,
  definition, outline, event-handler stubs; needs Location ranges, an
  in-memory source, a tolerant parse at the cursor, a small JSON
  reader/writer under the standard-library-only rule), then, only if the
  CodeLLDB route proves too raw, our own DAP adapter over lldb/gdb (BASIC
  names, temporaries hidden, BASIC watch expressions).
  Step 5.1 done (28 Oct 2026): the every-control demo, examples/demo
  (demo.vbp: frmDemo with menus and an event log, frmControls, frmLists,
  frmTimer, events.bas's Note/FocusNote). tests/run/demo.vbp builds the
  same files (the Timer form is left out: it shows the time); run tests
  take an optional `.size` file. Found and fixed: TopIndex past the end
  of a short ListBox blanked it. Jim then settled that a MsgBox or
  InputBox closing does not fire the form's Activate (as VB6):
  `come_to_top` never makes a dialog the `active` form. (The dialogs run
  test had been dumping only "Please resize": it now has a .size.)
  Option-key hint (Jim, 28 Oct 2026): `option_hint` in tf_forms.c, first
  in form_key after the mouse; a one-time MsgBox when an Option-layer
  symbol arrives outside a TextBox or with the menus open.
  Step 5.2 done: the Mid$ statement (sema `checkMidStatement` turns the
  assignment into a library call `Builtin::MidAssign`, parameter kind R
  = a String passed by address via codegen's `address()`; runtime
  tf_str_mid_assign).
  Declare settled (Jim, 28 Oct 2026; REFERENCE.md "Declare: calling
  C"): Lib "x.c" = a C file of the program (compiled with it), Lib "x" =
  a library to link, Lib "" = the C library; ByVal default; tfc writes
  the prototype (int32_t, int64_t, double, uint8_t, int); Strings as
  UTF-8 const char * (results copied), ByRef String a writable buffer;
  ByRef elements and numbers-only Types as pointers; no callbacks.
  Also (same day): Declare-only types CLong, CULong, CSizeT (C's long,
  unsigned long, size_t; a Long in BASIC, Overflow if it does not fit);
  a built-in read-only String `Platform` ("Windows", "macOS", "Linux",
  set by the C compiler for the target); conditional compilation (#If)
  after v1.
  Step 5.3 done (28 Oct 2026): Declare. Parser: `parseDeclare` (Declare,
  Lib, Alias are words, not reserved) makes a body-less Procedure
  (`isDeclare`, `lib`, `cName`; params' `cType` for CLong/CULong/
  CSizeT). Sema `checkDeclare` (types C can take, C name rules,
  one prototype per C function). Codegen: `declareSignature` (extern
  prototypes), `declareCall`/`declareResult` (Strings via tf_str_cstr /
  tf_c_buffer..tf_c_unbuffer / tf_str_from_c; CLong via tf_c_long etc.,
  checked before the call). Driver: `gatherLibs` (C files next to the
  module, libraries), `compileOwnCCommand` (their C compiled first, its
  messages shown), libraries linked; a failed build with Declares shows
  the C compiler's messages. Runtime tf_c.c (and tf_platform_name for
  Platform). Run test tests/run/declare (+ declare/utils.c).
  Step 5.4a done (28 Oct 2026): debugging. debug/lldb/termforms.py and
  debug/gdb/termforms.py (String summaries, tf_array children: [i], [i,j],
  Dictionary keys, Ref elements by their record's Type via a new
  `tf_record_ops.name`; step-avoid/skip of ^tf_ and the Types' helpers).
  Fixed the line mapping: a procedure's opening lines are pinned to its
  own #line (`pinPrologue`; they used to count as the next BASIC lines),
  main's plumbing is in the C file's own lines (`glue()`), TF_LINE goes
  inside a statement's block, and debug builds pass -gno-column-info, so
  a BASIC line is one breakpoint location. tests/tools/tf_debug.py
  (ctest label debug; lldb on macOS, gdb on CI's Linux gcc job, which
  installs it; skipped elsewhere and on Windows).
  Step 5.4b (28 Oct 2026): editors/vscode, a declarative VS Code
  extension (no code): TextMate grammars for .bas and .vbp, language
  configuration (comments, indentation, folding), breakpoints in .bas,
  the `$tfc` problem matcher, settings termforms.tfc and termforms.home;
  templates/tasks.json and launch.json (CodeLLDB, terminal integrated,
  the printers imported). Installed by linking the folder into
  ~/.vscode/extensions (README). Its JSON and regexes are checked; trying
  it in VS Code is Jim's (VS Code is on the Mac, CodeLLDB not yet).
  Tried by Jim in VS Code 1.143 with CodeLLDB (28 Oct 2026): highlighting,
  the build task and Problems list, breakpoints in .bas files, the call
  stack in BASIC, Locals and Static readable (controls as "cmdQuit
  (CommandButton)", literals as text; summaries registered for both the
  structs and their pointers, as CodeLLDB shows a pointer as its target).
  F5 must use the project configuration for a multi-file program.
  **Step 5.4 is complete.**
  Step 5.5 done (28 Oct 2026): examples/addressbook (book.bas +
  people.bas: a Contact Type in a Dictionary, addresses.txt, Find as
  you type, Store/Clear, menus, delete and save prompts), examples/viewer
  (viewer.bas: Dir$, FileLen, FileDateTime, lines in a ListBox, line
  numbers toggled), examples/samples (select_case, types, errors,
  arrays, strings, dictionary), examples/README.md. Each is a run test
  built from the example's own files. Run tests may have a NAME.files
  folder, copied to where the program runs and dated 2 Jan 2026 03:04:05
  local time (FileDateTime stays the same everywhere).
- **Release archives (5.6), settled with Jim 28 Oct 2026:** archives for
  the three platforms (macOS arm64, Linux x86-64, Windows x64); the
  Linux tfc static (musl, built with zig) so it runs on any x86-64
  Linux; pushing a tag `v*` makes CI build them and publish a GitHub
  Release (pre-release), every push building them as artifacts, the first
  tag v0.9.0 (the Phase 5 gate's candidate; v1.0.0 at Phase 6); macOS
  unsigned, the README saying to download with curl or clear the
  quarantine with xattr; contents: bin/tfc, runtime/ (sources and
  headers), debug/, editors/vscode, examples/, docs/ (guide, REFERENCE,
  LANGUAGE), licences and THIRD_PARTY_NOTICES; installed by unpacking
  anywhere and adding bin to PATH, tfc finding runtime/ beside its bin;
  Windows users pointed to zig via `winget install zig.zig` (the gate
  checks that a zig newer than the pinned 0.16 works). The repository is
  public, so Release files download without a GitHub login.
  Step 5.6 built (28 Oct 2026): tfc looks for an unpacked release's
  runtime beside its bin (`executablePath()` in build.cpp: macOS
  _NSGetExecutablePath, Linux /proc/self/exe, Windows GetModuleFileNameW)
  after --runtime and $TERMFORMS_RUNTIME, before the build's own source
  tree; tools/package.py lays out and packs an archive (git-tracked files
  only); tools/release/README.md goes inside; tools/release/smoke_test.py
  unpacks one and builds and runs hello with it;
  .github/workflows/release.yml builds the three on every push to main
  (artifacts) and publishes a pre-release on a v* tag matching
  project(VERSION). docs/RELEASING.md says how to cut one. The Linux tfc
  is cross-built with zig (on a Mac CMake also needs zig's ar/ranlib).
  No release published yet: v0.9.0 waits for Jim's go-ahead.
  Step 5.7 done (28 Oct 2026): docs/GUIDE.md, the programmer's guide
  (getting started, coming from VB, the language, forms, the library,
  Declare, building/debugging/deploying). The `docs.guide` test
  (tests/tools/tf_guide.py) builds every ```vb block in it that has a
  form block or Sub Main: keep the guide's complete programs complete,
  and its fragments without either. tfc's usage text now shows several
  .bas files, a .vbp and the runtime beside bin.
  Version 0.9.0 (28 Oct 2026, Jim's go-ahead): the version bumped in
  the three places, goldens regenerated, tag v0.9.0 pushed to publish
  the first pre-release.
  5.8 begun (Jim, 28 Oct 2026, on his wife's laptop: 10th-gen i5, 16 GB):
  the v0.9.0 Windows zip from GitHub, zig 0.17 from winget, bin added to
  PATH; hello.bas built and ran. So a zig newer than the pinned 0.16
  works. Both the winget install and the first tfc build were slow there
  (likely zig building its MinGW libc and compiler-rt into its cache on
  first use, Defender scanning, and tfc compiling the whole runtime on
  every build); to measure: a second build of the same program. A
  possible later improvement: cache the runtime's objects per target.
  The environment doc now starts with "Setting up from a release"
  (Windows, macOS, Linux: download, PATH, zig or the system compiler,
  build the examples, ship to Linux with chmod +x, a console check), and
  the project plan's status, v1 table and post-v1 list are current (29
  Oct 2026). Additions to v1 are recorded in the plan first (its risks
  table says so).
- **Next:** finish 5.8: Jim runs the environment doc's "Setting up from a
  release" on the laptop, as written. The terminal matrix is done except
  item 5 (Visual Studio debugging zig builds), which can be answered on
  Jim's own ThinkPad (it has development tools) whenever suits.

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
  (user identifiers, built-in names, properties, events), except module
  names, which ignore case (Jim, 28 Oct 2026: they come from file names,
  and Windows and macOS file names ignore case). Warn when a Sub
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
  programs carry no obligation). The Unicode width tables are generated
  (`tools/gen_width_tables.py`) and their notice is in
  `runtime/THIRD_PARTY_NOTICES` (see `docs/LICENSING.md`). Every source
  file starts with an SPDX line.

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
  debug/      sample.bas for tools/tf_debug.py (lldb, gdb)
              (+ .stderr = must fail with that message;
              + .args = command-line arguments, one a line;
              + .with = further modules, paths one a line;
              + .size = the headless screen, e.g. 80x24;
              + .files/ = files copied to where it runs;
              key scripts: {WAIT n} for Timers, {ALT+x}, {F4}, {CTRL+F4},
              {SHIFT+LEFT}, {CLICK x,y} and the mouse, {IDLE} for DoEvents)
  runtime/    C tests linked against tfrt
debug/{lldb,gdb}/termforms.py              debugger printers (Strings, arrays)
editors/vscode/                            VS Code extension and templates
tools/package.py, tools/release/           release archives (README, smoke test)
examples/hello.bas                         the vertical-slice program
examples/demo/                             every control, menus, dialogs, mouse
examples/{addressbook,viewer,samples}/     the other examples (README.md)
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
- **Arrays in C:** a `tf_array` value per array (locals `= {0}`, globals
  static), parameters `tf_array *`; fixed ones `tf_array_init` at
  procedure entry (globals in `main`), all locals `tf_array_free` at End
  Sub. Elements via `tf_array_at1/at2/atn` (checked; a bad index returns a
  scratch element so the statement's check can run). An element passed
  ByRef: its address is taken and checked first, then the array is
  locked around the call (`lockedAround`). Sema turns `a(i)` Calls into
  `ExprKind::Index`; a bare array name is accepted only via `wholeArray`.
- **Types in C:** a struct `t_<Type>` (members `f_<name>`) with generated
  `_init/_free/_copy` (void * signatures; copy builds a fresh record then
  moves it in, so copying from inside itself is safe) and `_busy` (an
  array in it is locked: copy raises 10); `t_<Type>_ops` (tf_record_ops)
  only for Types some array holds (TF_ELEM_RECORD). ByVal record params
  arrive as `const t_X *a_<name>` and are copied into local `v_<name>`;
  Functions return records by value (caller frees the temporary). The
  per-statement `releases` list holds whole C statements
  (`tf_str_release(t1);`, `t_X_free(&t2);`). Codegen paths: `lvalue()`,
  `address()`, `wholeArrayRef()`; a ByRef path through arrays locks each.
- **Refs in C:** `t_X *` pointing at a record behind a header (count,
  ops) from `tf_ref_new`; NULL is Nothing. Owned/borrowed as strings:
  stores use `v = tf_ref_replace(v, owned)`; ByVal Ref params retained on
  entry; `tf_ref_get(r, ops)` derefs (91 + scratch record for Nothing).
  Reads of a reachable Ref before a later call are held (retained);
  a ByRef path through a Ref holds the record for the statement.
  `opsOf()` records which Types' ops are emitted (arrays, New, get).
- **For Each:** lowered by sema (`checkForEach`) with With's machinery:
  the array path anchored once into `setup`, a hidden Long counter, the
  condition `n < Count(path)` (`Builtin::Count`, internal), and the body
  starting with `x = path(n)` (an Index with `linear`, which codegen emits
  as `tf_array_atlin`) and `n = n + 1`. Codegen emits setup, then emitDo.
- **Dictionary in C:** a `tf_array` (rank 1, its values in insertion
  order) plus `dict`, an index of keys (tf_array.c "Dictionaries"), so
  array code declares, frees, copies, locks and holds it in records. A
  Variable has `isDictionary` with rank 1 / dynamic; `d(k)` is an Index
  with `byKey` (read: `tf_dict_get`, error 5); `d(k) = v` and `d.Add`
  (an Assign whose target has `adding`) go through `dictionaryStore`,
  which parks the whole value before `tf_dict_put/add`, as adding may
  move the values. d.Count/Exists/Keys/Items/Remove/Clear become
  `Builtin::Dict*` calls with the Dictionary as args[0]; Remove and Clear
  are `StmtKind::Method`. Keys/Items are only valid where `checkWhole`
  allows a whole array (array assignment, For Each, which copies them
  into a hidden array first).
- **With:** no codegen of its own. Sema (`checkWith`/`anchorWith`) moves
  the parts of the target evaluated once (the last Ref, Function call or
  New on the path, then later indexes) into assignments to hidden locals
  `_with<n>` (no BASIC name starts with `_`) at the front of the With's
  body; each `.Member` becomes a clone of the remaining path
  (`clonePath`) plus the member, checked as usual. `withCopy` marks a
  copy of a Function's Type result as read-only.
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
- **Lenovo ThinkPad, Windows 11 x64**: Jim's; has had other development
  work on it (so not a clean machine). For Windows testing and the Visual
  Studio debugging question.
- **Jim's wife's laptop, an older ThinkPad, Windows 11**: nothing but
  Office 365; the clean machine for the Phase 5 gate. Windows terminal
  checks done on it (28 Oct 2026).

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
buffer is reused by the next call (copy what you keep). MSVC's C4701
("potentially uninitialized") only shows in CI's Windows jobs: initialise
a variable that is filled in only when a call succeeds.

## Known gaps (28 Oct 2026; ask Jim before filling them)

- No ComboBox, multi-line TextBox, ScrollBar (after v1, on tf_edit and
  the overlay); no MouseDown/Up/Move events; the clipboard is the
  program's own.
- Combining marks are not drawn (a cell holds one character); text
  that relies on them shows the base letters.
- **Post-v1 (Jim, 29 Oct 2026): file and folder dialogs**, built at run
  time from controls as MsgBox/InputBox are, listing the file system of
  the machine the program runs on (over SSH, the server's); after the
  ComboBox (for a file-type filter). About 1-2 days.
- **Settled, to build (Jim, 29 Oct 2026; REFERENCE.md "Arguments and
  the program's path", "Console programs"):** `CommandArgs()` (arguments
  only), App.Path/App.EXEName/App.FullPath (from the OS), no raw command
  line; Print to stdout and Print #0 to stderr (held while a form or
  dialog is on screen, written when the terminal is restored), `Line
  Input [prompt;] var` from stdin with EOF(0) (error 900 while a form is
  shown), `End n` (0-255) for the exit code.
  Arguments and App done (29 Oct 2026): `Builtin::CommandArgs` (an
  array like Split: `isDictionaryList`, codegen's array assignment calls
  `tf_command_args`; it also makes main take argc/argv); `App.X` becomes
  a call of `Builtin::AppPath`/`AppExeName`/`AppFullPath` in sema
  (`resolveAppMember`); `App` claimed like Err. Runtime: the platform
  layer now gives the arguments as a list (`tf_platform_args`; Windows
  splits GetCommandLineW itself) and `tf_platform_exe_path`. Run test
  tests/run/command_args (arguments with spaces, quotes, backslashes,
  an empty one, non-ASCII). Found then fixed (Jim, 29 Oct 2026): a
  method statement whose first argument starts with a parenthesis,
  `lst.AddItem (n + 1) & "x"`, did not parse (Subs already did, by
  `parenthesisedToEnd`); `parseAssignOrMethod` now parses the target
  again with that "(" left for the argument (`noListAt_`). Run test
  tests/run/paren_args.
  Console programs done (29 Oct 2026): parser `parseConsolePrint`
  (Print not followed by = or .; `printItems` shared with Print #),
  `parseConsoleLineInput` (the first part is the prompt if `;` follows),
  `End n` (End then a non-keyword: StmtKind::End with an `EndWith` call
  in args); Builtin::ConsolePrint/ConsoleLineInput/EndWith. Runtime in
  tf_file.c "Standard input and output": Print #0 and EOF(0) are file
  number 0; text held while `tf_console_screen(1)` (tf_forms.c ui_open/
  ui_resume set it, ui_suspend/ui_shutdown clear it and so write it out,
  after a headless dump); EOF(0) reads a line ahead. Platform:
  tf_platform_console_write/read_line (Windows: WriteConsoleW/
  ReadConsoleW for a console, bytes otherwise). Run tests may have a
  `.stdin` (else empty input) and `.exit` (the expected exit code);
  `.keys` is optional. Tests: run/console, run/console_forms,
  errors/console, errors/console_print. Also later, perhaps: a graphical form designer (Python Tk) writing the form
  block, with tfc printing its control catalog as JSON and a headless
  preview; open questions: .frm or rewrite in .bas, keeping comments.

## Next work (suggested order — confirm with Jim)

Phase 3, language core:
1. ~~Loops, Exit, Function, parameters, block scope~~ (done 27 Oct 2026).
2. ~~Long and Double, conversions, Option Strict widening~~ (done 27 Oct 2026).
3. ~~Try/Catch/Finally, Err, Throw~~ (done 27 Oct 2026).
4. ~~Const and Enum~~ (done 27 Oct 2026).
5. ~~Arrays, Types and Ref, With, For Each, Dictionary~~ (done 27 Oct 2026).
6. ~~Strings, Byte, text and binary files, the file system, Command$,
   Environ$, Shell, time~~ (done 28 Oct 2026).
7. ~~Multiple modules and the `.vbp` project file~~ (done 28 Oct 2026).

Not planned (Jim, 27 Oct 2026): warnings for a local assigned but never
read. Not worth the effort for TermForms' use; do not propose it again.

Phase 4, TUI runtime: ~~Label, CheckBox, OptionButton, ListBox, Frame,
Timer; menus; MsgBox/InputBox and a modal form stack; edit-buffer TextBox;
SGR mouse; Unicode width tables~~ (done 28 Oct 2026).

Each step: unit tests for rules/messages, golden C for codegen shape, run
tests for behaviour; update `docs/LANGUAGE.md` and `docs/REFERENCE.md`.
Generated C must compile clean with `-Wall -Wextra -Wpedantic -Werror`
(check the golden `.c` files by hand when codegen changes).
