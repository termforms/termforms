# TermForms BASIC: what works today

This page tracks the language as implemented, not as planned. It grows with
each phase; the full design lives in the project's design documents.

Version 0.0.1 is the **vertical slice**: enough language to build one form
with a TextBox and a CommandButton and react to a click.

## Two rules that differ from VB

- **Identifiers are case-sensitive.** `count` and `Count` are different
  variables. Keywords are not: `dim`, `Dim` and `DIM` all work. The compiler
  warns when a Sub looks like an event handler but its case does not match
  the control, because such a Sub would never be called.
- **Arrays will be 0-based** (when they arrive): `Dim a(10)` gives 11
  elements, `a(0)` to `a(10)`. There is no `Option Base`.

## Program structure

A source file holds at most one form block, module-level `Dim` statements and
Subs. A program needs either a form or a `Sub Main`.

```vb
Begin Form frmHello            ' Width and Height include the border
    Caption = "Hello"
    Width   = 40
    Height  = 8
    Begin TextBox txtName      ' Left and Top are inside the border
        Left  = 2
        Top   = 1
        Width = 34
    End
    Begin CommandButton cmdGreet
        Caption = "&Greet"
        Left    = 2
        Top     = 3
        Width   = 12
    End
End

Sub cmdGreet_Click()           ' wired to the Click event by its name
    txtName.Text = "Hello, " & txtName.Text
End Sub
```

## Supported so far

| Area | Supported |
| --- | --- |
| Types | `Integer` (32-bit, overflow is a runtime error), `String` (UTF-8), `Boolean` |
| Declarations | `Dim x As Integer`, `Dim a, b As String` (both String), suffixes `%` and `$` |
| Statements | assignment, `If ... Then ... ElseIf ... Else ... End If`, single-line `If`, calling a Sub (`Name` or `Call Name`), `End` |
| Operators | `&` (Integers and Booleans become text), `+ - *`, `\` (integer division), `= <> < > <= >=`, `And Or Not`, unary `-` |
| Controls | `Form` (Caption, Width, Height; event Load), `TextBox` (Text, Left, Top, Width, Height; event Change), `CommandButton` (Caption, Left, Top, Width, Height; event Click) |
| Comments | `'` and `REM`; ` _` at the end of a line continues it |

Option Explicit is always on and Option Strict is on: no implicit
conversions except through `&`. `/` and `Long`/`Double` are reported as not
supported yet.

## Keys in a running program

| Key | Action |
| --- | --- |
| Tab, Shift+Tab | Next / previous control |
| Enter or Space | Press the focused button |
| Left, Right, Home, End, Backspace, Delete | Edit in a TextBox |
| Ctrl+Q | Close the form |

## Building programs

```sh
tfc hello.bas                    # native build with the system C compiler
tfc --cc "zig cc" hello.bas      # the same, with zig
tfc --cc "zig cc -target x86_64-linux-musl" -o hello-linux hello.bas
                                 # static Linux binary from any machine
tfc --emit-c -o - hello.bas      # show the generated C
```

`TFC_CC` sets the default compiler. Generated C carries `#line` directives,
so a debugger steps through the `.bas` file directly.

## Headless mode (for tests)

When `TERMFORMS_HEADLESS` is set, a program reads keys from it instead of the
terminal and prints the final screen as text when the form closes.
`TERMFORMS_HEADLESS_SIZE` sets the screen size (default `80x25`).

```sh
TERMFORMS_HEADLESS='Jim{TAB}{ENTER}' ./hello
```

Special keys are written `{ENTER}`, `{TAB}`, `{BACKTAB}`, `{BKSP}`, `{DEL}`,
`{LEFT}`, `{RIGHT}`, `{UP}`, `{DOWN}`, `{HOME}`, `{END}`, `{ESC}`,
`{CTRL+Q}`; `{{` is a literal brace.
