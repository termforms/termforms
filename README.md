# TermForms

TermForms is a small, cross-platform language with familiar VB-DOS-style syntax
for building event-driven, text-mode forms applications that run in a terminal,
locally or over SSH. Programs compile to plain C99 and then to a single native
executable; on Linux that can be one static binary you copy to a server and run.

The language itself is referred to as **TermForms BASIC**. It is deliberately
*not* a Visual Basic clone: the syntax aims to feel at home to anyone who wrote
VB-DOS or QuickBASIC, while dropping what made those hard to maintain
(Variants, `On Error Resume Next`, implicit declarations).

## Status

**Nearing v1.** The language core, the forms runtime (every v1 control,
menus, dialogs, several forms, the mouse), files, calls into C and source-level
debugging work on Linux, macOS and Windows, including static Linux binaries
cross-compiled with `zig cc`. Start with the programmer's guide,
[`docs/GUIDE.md`](docs/GUIDE.md); [`docs/LANGUAGE.md`](docs/LANGUAGE.md) lists
what works today and [`docs/REFERENCE.md`](docs/REFERENCE.md) has every rule.

## Planned shape (v1)

- **Language:** Integer (32-bit), Long (64-bit), Double, String (UTF-8), Boolean;
  user-defined Types and reference-counted `Ref` types; Sub/Function;
  `Try`/`Catch`/`Finally`; `Option Explicit` always on, `Option Strict` on by default.
- **Forms:** Label, TextBox, CommandButton, CheckBox, OptionButton, ListBox,
  Frame and Timer controls; menus; `MsgBox` and `InputBox`; handlers wired by the
  familiar `Sub Control_Event` naming convention.
- **Platforms:** Linux, macOS (Apple Silicon) and Windows x64, using one
  VT-sequence terminal layer on all three.
- **Toolchain:** the compiler emits C and drives clang, gcc, MSVC or `zig cc`;
  `zig cc` also cross-compiles static Linux binaries.
- **Debugging:** generated C carries `#line` directives, so gdb, lldb and
  Visual Studio step through the BASIC source.

A flavour of what a program is expected to look like (illustrative only; the
syntax is not final):

```vb
' hello.bas
Begin Form frmHello
    Caption = "Hello"
    Width   = 40
    Height  = 8
    Begin TextBox txtName
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

Sub cmdGreet_Click()
    MsgBox "Hello, " & txtName.Text
End Sub
```

## Building

Requirements are in [`toolchain.txt`](toolchain.txt).

```sh
# macOS and Linux
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

# Windows (Visual Studio 2022 or later)
cmake --preset vs
cmake --build --preset vs
ctest --preset vs
```

Then try the example:

```sh
cd examples
../build/dev/compiler/tfc hello.bas
./hello
```

See [`docs/CODING_STANDARDS.md`](docs/CODING_STANDARDS.md) before contributing.

## Licence

- The compiler, tools and everything else in this repository are under the
  **BSD 2-Clause** licence — see [`LICENSE`](LICENSE).
- The runtime library in [`runtime/`](runtime/), which is linked into every
  compiled program, is under the **Zero-Clause BSD (0BSD)** licence — see
  [`runtime/LICENSE`](runtime/LICENSE). Programs you build with TermForms carry
  no licence or attribution obligation from it.
