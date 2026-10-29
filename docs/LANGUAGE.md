# TermForms BASIC: what works today

This page tracks the language as implemented, not as planned. It grows with
each phase; the full design lives in the project's design documents.

Version 0.9.0, the first pre-release, has the whole v1 language core, the
forms runtime (every v1 control, menus, dialogs, several forms, the mouse),
files and the file system, and Declare for calling C. For learning the
language, read the guide, `docs/GUIDE.md`; `docs/REFERENCE.md` has every
rule as decided.

## Rules that differ from VB

- **Identifiers are case-sensitive.** `count` and `Count` are different
  variables. Keywords are not: `dim`, `Dim` and `DIM` all work. The compiler
  warns when a Sub looks like an event handler but its case does not match
  the control, because such a Sub would never be called.
- **Arrays are 0-based**: `Dim a(10)` gives 11 elements, `a(0)` to
  `a(10)`. There is no `Option Base`, and no `Dim a(1 To 10)`.
- **Parameters are ByVal by default** (VB.NET, not VB6). Write `ByRef` in
  the signature to let a Sub change the caller's variable.
- **A `Dim` inside a block** (If, For, Do, While) is visible only inside it,
  and inside a loop it starts fresh on every pass.

## Program structure

A source file holds at most one form block, module-level `Dim` statements,
Subs and Functions. A program needs either a form or a `Sub Main`.

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
| Types | `Integer` (32-bit), `Long` (64-bit), `Double`, `String` (UTF-8), `Boolean`; overflow is a runtime error |
| Declarations | `Dim x As Integer`, `Dim a, b As String` (both String), suffixes `%` `&` `#` `$`; values: `Dim n As Integer = 5`, `Dim days() As String = {"Mon", "Tue"}`, `Dim g(,) As Integer = {{1, 2}, {3, 4}}`, module level too (set at start) |
| Statements | assignment, `If ... Then ... ElseIf ... Else ... End If`, single-line `If`, `Select Case` (values, `a To b`, `Is > x`, `Case Else`), calls, `For ... Next`, `Do ... Loop`, `While ... Wend`, `Exit`, `Return`, `Try ... Catch ... Finally`, `Throw`, `Err.Raise`, `Err.Clear`, `End` |
| Procedures | `Sub` and `Function` with ByVal (default) and `ByRef` parameters; recursion |
| Constants | `Const` (module level or in a procedure), `Enum ... End Enum` (Integer or `As Long`), `CType(value, type)` |
| Arrays | fixed `Dim a(9)`, `Dim g(24, 79)`; dynamic `Dim a()`, `Dim t(,)` with `ReDim [Preserve]`; `Erase`, `UBound`, `LBound`; array parameters |
| Types | `Type ... End Type` records with scalar, array and Type members; arrays of Types; Type parameters and Function results |
| Refs | `Ref T`, `New T`, `Dim r As New T`, `Nothing`, `Is` / `IsNot`; Ref members, arrays of Refs |
| With | `With ... End With` on a Type, an element, a Ref, a Function's result or a control; nested |
| For Each | over arrays (any number of dimensions), `d.Keys`, `d.Items`; `For i As Integer = ...` and `For Each w As String In ...` declare the variable for the loop alone |
| Files | `Open ... For Input/Output/Append/Binary As #n`, `Print #`, `Line Input #`, `Get #`, `Put #`, `Seek #`, `Close`, `EOF`, `LOF`, `Seek`, `FreeFile` |
| File system | `Kill`, `MkDir`, `RmDir`, `ChDir`, `CurDir$`, `Name ... As`, `FileCopy`, `FileLen`, `FileDateTime`, `Exists`, `Dir$` |
| Controls | Label, TextBox (SelStart, SelLength, SelText, MaxLength, Locked, PasswordChar; selection, words, undo, cut/copy/paste), CommandButton, CheckBox, OptionButton, Frame (holding controls), ListBox (AddItem, RemoveItem, Clear, List(i), Selected(i), MultiSelect, Sorted), Timer (Interval, Enabled, the Timer event); Default/Cancel buttons; Enabled, Visible; access keys (Alt+letter) |
| Forms | several (one per module): `Show [vbModal]`, `Hide`, `Load`, `Unload`, `Me`, `DoEvents`, `Visible` (read only); events Load, Unload(ByRef Cancel), Activate; GotFocus, LostFocus, KeyPress(ByRef KeyAscii), KeyDown(ByRef KeyCode, Shift) |
| Menus | `Begin Menu` blocks: a menu bar with drop-downs (two levels); Caption (`-` a separator), Enabled, Visible, Checked, Shortcut; the Click event; F10 or Ctrl+B, Alt+letter |
| Dialogs | `MsgBox` (function or statement), `InputBox`; VB's named constants (vbYesNo, vbQuestion, vbYes, vbCrLf, ...) |
| Modules | several `.bas` files (`tfc main.bas stock.bas`); `Public`/`Private`; `Module.Name`; `frmMain.control` from other modules |
| Console | `Print` (standard output), `Print #0` (standard error), `Line Input [prompt;] var` (standard input), `EOF(0)`, `End n` (the exit code) |
| System | `Command$`, `CommandArgs()`, `App.Path`/`EXEName`/`FullPath`, `Environ$`, `Shell`, `Now`, `Date$`, `Time$`, `Timer` |
| Strings | `Len`, `LenB`, `Mid$` (and the statement `Mid$(s, 2, 1) = "x"`), `Left$`, `Right$`, `InStr`, `InStrRev`, `LCase$`, `UCase$`, `LTrim$`, `RTrim$`, `Trim$`, `Space$`, `String$`, `Chr$`, `Asc`, `Replace`, `StrReverse`, `Split`, `Join` |
| Dictionary | `Dictionary(Of T)` with String keys: `d(k)`, `Add`, `Exists`, `Remove`, `Count`, `Clear`, `Keys`, `Items`; as a variable, parameter or Type member |
| Operators | `&` (numbers and Booleans become text), `+ - *`, `/` (always a Double), `\` (integer division), `Mod`, `^`, `= <> < > <= >=`, `And Or Not`, unary `-` |
| Functions | `CInt`, `CLng`, `CDbl`, `CStr`, `Str$`, `Val`, `Fix`, `Int` |
| Controls | `Form` (Caption, Width, Height; event Load), `TextBox` (Text, Left, Top, Width, Height; event Change), `CommandButton` (Caption, Left, Top, Width, Height; event Click) |
| Text on screen | UTF-8; wide characters (CJK, emoji) take two cells; combining marks are not drawn |
| Comments | `'` and `REM`; ` _` at the end of a line continues it |

Option Explicit is always on and Option Strict is on: widening is implicit
(Integer → Long → Double), but narrowing needs `CInt` or `CLng`, and text
and numbers convert only explicitly (`CStr`, `Val`, `CDbl`...) or through
`&`.

## Numbers

```vb
Dim n As Long, d As Double, i%
n = 5000000000                  ' too big for an Integer, so a Long literal
d = 10 / 4                      ' / always gives a Double: 2.5
i = CInt(d)                     ' 2: halves round to even, as in VB
i = 7 \ 2 + 7 Mod 2             ' 3 + 1
d = 2 ^ 10 + &HFF + 1.5E-3      ' 1024 + 255 + 0.0015
Dim b As Byte                   ' 0 to 255
b = 200                         ' a constant that fits needs no CByte
i = b * 2                       ' Byte arithmetic is done as Integer: 400
b = CByte(i \ 4)                ' and needs CByte to store back
```

- A Double prints as the shortest text that reads back exactly: `0.1`,
  `2.5`, `1E+20`, `0.30000000000000004` (for 0.1 + 0.2). `Str$` adds VB's
  leading space for zero and positive numbers: `Str$(5)` is `" 5"`.
- Division by zero, a result too large and `CInt` of a value out of range
  are runtime errors; a Double is never Infinity or NaN.
- `CInt("12")`, `CDbl("1.5")` need the whole text to be a number (else
  error 13, Type mismatch); `Val("12abc")` is lenient and gives 12.
- `^` binds tighter than unary minus: `-2 ^ 2` is -4.
- For loops work over Long and Double variables too: `For d = 0 To 1 Step 0.25`.

## Constants and Enums

```vb
Const MAX_ITEMS = 100               ' an Integer, from its value
Const TITLE = "Orders " & MAX_ITEMS  ' worked out when compiling
Const RATE As Double = 7            ' As (or a suffix) picks the type

Enum Status
    Open                            ' 0
    Paid                            ' 1
    Cancelled = 9
End Enum

Dim s As Status
s = Status.Paid                     ' members are always written with the Enum
If s = Status.Paid Then n = s + 1   ' a Status works as its number
s = CType(n, Status)                ' a number becomes a Status only with CType
```

- A Const's value may use literals, other Consts above it, Enum members
  and operators, but not variables or functions. An overflow there is a
  compile error.
- An Enum is its own type: a `Status` variable takes only Status values,
  and Status and another Enum cannot be compared. As text an Enum value is
  its number.

## Arrays

```vb
Dim scores(9) As Integer            ' fixed: scores(0) to scores(9)
Dim names() As String               ' dynamic: empty until ReDim
ReDim names(2)
names(0) = "Ann"
ReDim Preserve names(UBound(names) + 1)   ' grow, keeping the contents

Function Total(values() As Integer) As Integer   ' an array parameter
    Dim i As Integer
    For i = 0 To UBound(values)
        Total = Total + values(i)
    Next
End Function
```

- Only a dynamic array (`Dim a()`, `Dim t(,)` for two dimensions) can be
  ReDim'd; a fixed array's size is a constant. `b = a` copies the elements.
- An index outside the array is error 9. An array parameter is read-only
  unless it is ByRef.

## Types

```vb
Type Point
    X As Integer
    Y As Integer
End Type

Type Shape
    Name As String
    Corners() As Point                  ' an array member
End Type

Dim s As Shape, copy As Shape
s.Name = "square"
ReDim s.Corners(3)
s.Corners(2).X = 10
copy = s                                ' copies everything: they share nothing
```

- A Type is a value: assignment copies the record, arrays and nested Types
  included. A ByVal Type parameter is the callee's own copy; ByRef shares.
- Types cannot be compared with `=`; compare their members.
- `With ... End With` works on Types (see Refs below).

## Refs

```vb
Type Node
    Value As Integer
    Next As Ref Node
End Type

Dim head As Ref Node                    ' Nothing until given a record
Dim n As New Node                       ' a new record, made at the Dim
n.Value = 1
n.Next = head
head = n                                ' head and n share one record
While head IsNot Nothing
    total = total + head.Value
    head = head.Next
Wend
```

- A Ref shares its record; a Type value copies. `r = v` (a value into a
  Ref) makes a new record holding a copy; `v = r` copies the record out.
- Using a Ref that is Nothing is error 91. Compare Refs with `Is` and
  `IsNot`, not `=`.
- Records are freed when nothing refers to them any more, but records that
  refer to each other in a circle are never freed.

## Strings

```vb
s = "Hello, World"
Mid$(s, 8)            ' "World": positions start at 1, as in VB
InStr(s, "o")         ' 5; 0 when not found
parts = Split("a,b,c", ",")   ' parts() As String: UBound(parts) = 2
Join(parts, "+")      ' "a+b+c"
```

- Lengths and positions count characters, not bytes (`LenB` counts bytes).
- A bad argument (a start before 1, a negative length) is error 5.
- `LCase$` and `UCase$` change only A-Z and a-z for now.
- The `$` is optional (`Mid` = `Mid$`), except in `String$`.

## Files

```vb
Open "notes.txt" For Output As #1
Print #1, "n="; n                       ' "n=5": items joined with nothing between
Close #1
Open "notes.txt" For Input As #1
Do While Not EOF(1)
    Line Input #1, s
Loop
Close #1
```

- Files are UTF-8 text with LF line ends (CRLF is read too).
- Binary files: `Get #1, offset, buf [, count]` reads into a Byte array
  (offsets count from 0; leave the offset out to carry on from the last
  read); `Put` writes the same way; `Seek #1, offset` moves.
- `Open`, `Print`, `Input` and the rest are not reserved words: they are
  file statements only in these shapes, and free as names otherwise.

## The file system

```vb
MkDir "backup"
FileCopy "notes.txt", "backup/notes.txt"
name = Dir$("backup/*.txt")         ' each matching file in turn, sorted, then ""
Do While name <> ""
    total = total + FileLen("backup/" & name)
    name = Dir$()
Loop
If Exists("old.txt") Then Kill "old.txt"
Name "a.txt" As "b.txt"
```

## The system

```vb
code = Shell("make all")          ' runs it through the shell, waits: its exit code
home = Environ$("HOME")           ' "" when not set
args = Command$                   ' the arguments, joined by spaces
For Each a As String In CommandArgs()   ' the arguments one by one
cfg = App.Path & "/settings.txt"  ' the executable's folder
stamp = Now                       ' "2026-10-28 14:05:09"
```

## Console programs

```vb
Sub Main()                          ' no forms: standard input and output
    Line Input "Name: "; name       ' a line of standard input
    Print "Hello, "; name           ' ; joins; a trailing ; leaves off the newline
    Do While Not EOF(0)             ' the rest of the input (a pipe, a file)
        Line Input line
    Loop
    Print #0, "done"                ' standard error
    End 2                           ' exit code 2
End Sub
```

While a form is on screen, Print's text is held and written when the
terminal is normal again; Line Input there is error 900.

## Modules

```sh
tfc main.bas stock.bas reports.bas       # one program of three modules
tfc inventory.vbp                        # the same, from a project file
```

A project file lists the files, one a line, with the program's name:

```
Name=Inventory
Form=main.bas
Module=stock.bas
```

- A module is named after its file (`stock.bas` is `stock`). Its Subs,
  Functions, Types and Enums are Public (seen by every module) unless
  marked `Private`; its `Dim`s and `Const`s are Private unless declared
  `Public x As T`.
- `stock.Label()` names one module's `Label` when two have one. Module
  names ignore case (`Stock.Label()` is the same), as file names do on
  Windows and macOS.
- Other modules reach the form's controls through the form:
  `frmMain.txtName.Text`.

## Dictionaries

```vb
Dim ages As Dictionary(Of Integer)      ' keys are Strings; ready to use
ages("Ann") = 30                        ' adds, or replaces
ages.Add "Bo", 25                       ' error 457 if "Bo" is there
If ages.Exists("Cy") Then ages.Remove "Cy"
For Each k In ages.Keys                 ' in the order keys were added
    total = total + ages(k)
Next
```

- Reading a key that is not there is error 5; only `d(k) = v` and `Add`
  add one. Keys are case-sensitive.
- A Dictionary behaves like an array: `d2 = d` copies it, ByVal it is
  read-only, and it can be a member of a Type. `Keys` and `Items` give
  copies, as arrays (`Dim ks() As String: ks = d.Keys`).

## With

```vb
With customers(i)
    .Name = "Ann"
    With .Home
        .City = "Oslo"
    End With
    Bump .Age                       ' a space before the dot: a With member
End With
With txtName
    .Text = ""
End With
```

- The target is worked out once, at `With`: changing `i`, or making a
  Ref variable refer to another record, inside the With does not change
  what `.Name` means.
- An array element is looked up again at each use, so a `ReDim` that
  removes it makes the next `.Name` error 9.
- `With F()` on a Function returning a Type works on a copy, which cannot
  be changed; on a Function returning a Ref, the record is shared.
- `Name .X` (with a space) passes `.X` to the Sub `Name`; `Name.X` is a
  member of `Name`.

## Errors

```vb
Try
    ratio = total \ count
Catch When Err.Number = 11          ' Division by zero
    ratio = 0
Catch                               ' any other error
    txtStatus.Text = Err.Number & ": " & Err.Description & " (line " & Err.Line & ")"
Finally
    txtStatus.Text = txtStatus.Text & "."   ' runs however the Try ends
End Try

Throw 1000, "Customer not found"    ' raise your own error
Err.Raise 1000, "Lookup", "Customer not found"   ' the same, VB's form (with a source)
```

- Catch has no variable: the error is in `Err` (`Number`, `Description`,
  `Source`, `Line`). Inside a Catch, `Throw` on its own passes the error on.
- Errors pass up through Subs and Functions to the nearest Try; an error
  nobody catches ends the program with `file:line: runtime error N:
  message`, as before. `Err.Line` is the line where the error happened.
- Finally runs on every way out of the Try: normal end, an error, `Exit`
  or `Return`. A Finally cannot itself be left with `Exit` or `Return`.
- **`End` stops the program at once, even inside a Try: Finally blocks do
  not run.** Do any cleanup before the `End`.
- A statement that fails changes nothing: after a failed `x = a \ b`, x
  keeps its old value.
- Out of memory cannot be caught.

## Subs and Functions

```vb
Function Area(w As Integer, h As Integer) As Integer
    Area = w * h                ' or: Return w * h
End Function

Sub Swap(ByRef a As Integer, ByRef b As Integer)
    Dim t As Integer
    t = a
    a = b
    b = t
End Sub

Sub Form_Load()
    Dim x As Integer, y As Integer
    x = Area(3, 4)
    Swap x, y                   ' also: Swap(x, y) or Call Swap(x, y)
End Sub
```

- Parameters need a type (`As Integer` or a suffix); a Function needs a
  return type (`Function Name$()` or `As String`).
- A ByRef argument must be a variable of exactly the parameter's type.
  Changing a ByVal parameter inside the procedure does not reach the caller.
- Inside a Function its bare name is the result, on either side of `=`.
  `Name(...)` is always a call, so a parameterless Function calls itself
  with `Name()`. A Function that never sets its result returns 0, `""` or
  False.
- `Exit Sub`, `Exit Function` and `Return` leave early. A Function may be
  called as a statement; its result is discarded.
- Expressions are evaluated left to right, on every C compiler: a Function
  that changes a variable does not alter a value read from it earlier in
  the same expression.
- Event handlers and `Sub Main` take no parameters.

## Loops

```vb
For i = 1 To 10 Step 2          ' Step is optional (1); it may be negative
Next i                          ' the name after Next is optional

Do While n < 10                 ' or Do Until ...; or test at the bottom:
Loop                            '   Do ... Loop While / Loop Until ...

While n > 0                     ' VB-DOS spelling
Wend                            ' End While (VB.NET) also works

For Each w In words             ' each element of an array
Next
```

- The For variable is a numeric variable (Integer, Long or Double)
  declared beforehand. The start,
  limit and Step are evaluated once, before the first pass. After the loop
  the variable holds the first value past the limit (or the start, if the
  loop never ran). A loop that ends exactly at the largest Integer
  overflows on its last increment, as in VB.
- `Exit For`, `Exit Do` and `Exit While` leave the innermost loop of that
  kind, even from inside other loops.
- For Each gives its variable a copy of each element in turn (several
  dimensions: the first index changes fastest). A ReDim inside that
  shrinks the array ends the loop early.

## Controls

```vb
Begin Form frmMain
    Begin CheckBox chkBold
        Caption = "&Bold"           ' Alt+B toggles it
    End
    Begin Frame fraSize
        Caption = "Size"
        Top = 1
        Height = 4
        Begin OptionButton optSmall  ' Left and Top are inside the Frame,
            Caption = "&Small"       ' and the Frame's OptionButtons are a group
        End
    End
    Begin CommandButton cmdOK
        Caption = "OK"
        Top = 6
        Default = True               ' Enter presses it (Cancel = True: Esc)
    End
End
```

## Menus

```vb
Begin Form frmMain
    Begin Menu mnuFile               ' on the menu bar (the form's first row)
        Caption = "&File"
        Begin Menu mnuSave           ' an item in File's drop-down
            Caption = "&Save"
            Shortcut = "Ctrl+S"      ' also Ctrl+letter, F1-F12 with Shift/Ctrl/Alt, Del
        End
        Begin Menu mnuSep1
            Caption = "-"            ' a separator line
        End
        Begin Menu mnuWrap
            Caption = "&Wrap"
            Checked = True
        End
    End
End

Sub mnuFile_Click()                  ' runs as File opens: update the items here
    mnuSave.Enabled = dirty
End Sub
```

With a menu bar, controls' Top counts from the row below it.

## Keys in a running program

| Key | Action |
| --- | --- |
| Tab, Shift+Tab | Next / previous control (once into an OptionButton group) |
| Arrows | Move within an OptionButton group, selecting |
| Space | Press a button, toggle a CheckBox, select an OptionButton |
| Enter | Press the focused button, or the Default button |
| Esc | Press the Cancel button |
| Alt+letter | The control whose Caption has &letter |
| Left, Right, Home, End, Backspace, Delete | Edit in a TextBox (Shift+: select; Ctrl+Left/Right: by words) |
| Ctrl+A, Ctrl+X, Ctrl+C, Ctrl+V, Ctrl+Z | In a TextBox: select all, cut, copy, paste (the program's own clipboard), undo |
| The terminal's paste | Types the text in; line breaks become spaces (never presses Enter) |
| Ctrl+F4 (or Alt+F4) | Close the form |
| The mouse | Click to focus, press, toggle, choose, open menus; drag to select in a TextBox; double-click a word or a ListBox item; the wheel scrolls a ListBox. `TERMFORMS_MOUSE=0` turns it off; Shift+drag selects text on the screen as usual |
| F10 or Ctrl+B | Go to the menu bar (again, or Esc: leave it) |
| Alt+letter | Open the menu whose Caption has &letter (before a control's) |
| In the menus | Left/Right: menu to menu; Down/Enter: open; Up/Down: item to item; Enter or a letter: choose; Esc: back |

### Alt keys on a Mac

Access keys (Alt+letter) and the menus' Alt+letter need the terminal to
send the Option key as Alt (an Esc before the letter). Out of the box
macOS terminals type Option characters instead (˜, ¥), and no access key
works:

- **iTerm2**: Settings > Profiles > Keys: Left Option key: **Esc+**.
- **Terminal.app**: Settings > Profiles > Keyboard: **Use Option as Meta
  key**.
- **Ghostty**: Ghostty > Settings… opens its configuration file; add
  `macos-option-as-alt = left` (or `true` for both Option keys) and
  reload the configuration.

On a non-Apple keyboard the key to hold is the one macOS treats as
Option (often labelled "opt" or "alt/opt"). F10 or Ctrl+B reaches the
menus with no setting at all. A program notices the Option key typing characters
(on a button or a list, or in the menus) and says, once, how to set it.

## Calling C

```vb
Declare Function add_tax Lib "utils.c" (ByVal cents As Integer) As Integer
Declare Function sqlite3_libversion Lib "sqlite3" () As String
Declare Function strlen Lib "" (ByVal s As String) As CSizeT
```

`Lib "utils.c"` is a C file next to the `.bas` file, compiled with the
program; `Lib "sqlite3"` a library linked in; `Lib ""` the C library.
Parameters are ByVal unless ByRef (a pointer). C sees Integer as
`int32_t`, Long `int64_t`, Double `double`, Byte `uint8_t`, Boolean
`int`, a ByVal String `const char *` (UTF-8), a ByRef String a buffer it
may fill (`Space$(n)` first), an element passed ByRef (`buffer(0)`) a
pointer into the array, a Type of numbers passed ByRef a pointer to a
matching struct. `CLong`, `CULong` and `CSizeT` are C's `long`,
`unsigned long` and `size_t`, whose sizes differ by platform. `Alias
"cname"` when the C name differs. `Platform` is "Windows", "macOS" or
"Linux".

## Debugging

tfc builds with debug information by default (`--release` leaves it out),
and every BASIC statement is one line to a debugger: breakpoints go on
`.bas` lines, and stepping goes from BASIC line to BASIC line, into your
Subs and Functions and past the runtime's own. Load TermForms' printers
so Strings, arrays and Dictionaries show as BASIC would:

```text
lldb ./myprog                         (macOS; Linux too)
(lldb) command script import /path/to/termforms/debug/lldb/termforms.py
(lldb) breakpoint set -f main.bas -l 42

gdb ./myprog                          (Linux)
(gdb) source /path/to/termforms/debug/gdb/termforms.py
(gdb) break main.bas:42
```

In Visual Studio Code, `editors/vscode` adds highlighting, breakpoints
in `.bas` files and tfc's errors in the Problems list, and its templates
set up building and debugging with CodeLLDB (see its README).

Variables keep their C names: `x` is `v_x` in a Sub, `g_x` at module
level; a Type's members are `f_<name>`. A Boolean shows as 0 or 1. The
program needs a terminal of its own while it is debugged (lldb and gdb
share theirs; `TERMFORMS_HEADLESS` runs it without one).

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

Every mistake in a program is reported by `tfc` against the `.bas` file;
the C compiler's own messages are not shown (`-v` shows them, and the
command). If the C compiler cannot be run, `tfc` says so. If it runs and
fails, that is a fault in `tfc` (or the C compiler), never in the program:
`tfc` says so and keeps the generated C (`<output>.tf.c`) and the C
compiler's messages (`<output>.tf-log`) for a bug report.

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
