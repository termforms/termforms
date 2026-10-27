# TermForms BASIC: what works today

This page tracks the language as implemented, not as planned. It grows with
each phase; the full design lives in the project's design documents.

Version 0.0.1 is the **vertical slice**: enough language to build one form
with a TextBox and a CommandButton and react to a click. Phase 3 adds the
language core step by step; so far: Sub and Function with parameters, the
loops, Long and Double, Try/Catch, Const and Enum, arrays, and Types.
`docs/REFERENCE.md` has every rule
as decided, including the ones not built yet.

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
| Declarations | `Dim x As Integer`, `Dim a, b As String` (both String), suffixes `%` `&` `#` `$` |
| Statements | assignment, `If ... Then ... ElseIf ... Else ... End If`, single-line `If`, calls, `For ... Next`, `Do ... Loop`, `While ... Wend`, `Exit`, `Return`, `Try ... Catch ... Finally`, `Throw`, `Err.Raise`, `Err.Clear`, `End` |
| Procedures | `Sub` and `Function` with ByVal (default) and `ByRef` parameters; recursion |
| Constants | `Const` (module level or in a procedure), `Enum ... End Enum` (Integer or `As Long`), `CType(value, type)` |
| Arrays | fixed `Dim a(9)`, `Dim g(24, 79)`; dynamic `Dim a()`, `Dim t(,)` with `ReDim [Preserve]`; `Erase`, `UBound`, `LBound`; array parameters |
| Types | `Type ... End Type` records with scalar, array and Type members; arrays of Types; Type parameters and Function results |
| Operators | `&` (numbers and Booleans become text), `+ - *`, `/` (always a Double), `\` (integer division), `Mod`, `^`, `= <> < > <= >=`, `And Or Not`, unary `-` |
| Functions | `CInt`, `CLng`, `CDbl`, `CStr`, `Str$`, `Val`, `Fix`, `Int` |
| Controls | `Form` (Caption, Width, Height; event Load), `TextBox` (Text, Left, Top, Width, Height; event Change), `CommandButton` (Caption, Left, Top, Width, Height; event Click) |
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
- `Ref`, `New`, `Nothing` and `With ... End With` come next.

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
```

- The For variable is a numeric variable (Integer, Long or Double)
  declared beforehand. The start,
  limit and Step are evaluated once, before the first pass. After the loop
  the variable holds the first value past the limit (or the start, if the
  loop never ran). A loop that ends exactly at the largest Integer
  overflows on its last increment, as in VB.
- `Exit For`, `Exit Do` and `Exit While` leave the innermost loop of that
  kind, even from inside other loops.

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
