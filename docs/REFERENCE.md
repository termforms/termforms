# TermForms BASIC: language reference (as designed)

This is the reference for the language as **decided**, rule by rule, so
nobody (Jim included) has to remember which VB dialect a rule came from.
`LANGUAGE.md` tracks what the compiler implements today; this file is the
target. Each rule carries a status:

- **Settled**: decided with Jim; change only by a new decision.
- **Proposed**: the implementer's default, not yet confirmed by Jim.

When a rule differs from VB6 or VB.NET, it says so: those are the rules
that will catch people out.

## Rules most likely to surprise a VB programmer

| Rule | TermForms | VB6 | VB.NET |
| --- | --- | --- | --- |
| Identifier case | Case-sensitive (`count` ≠ `Count`) | Insensitive | Insensitive |
| Array bounds | `Dim a(10)` = 11 elements, `a(0)`..`a(10)`, no `Option Base` | Same, but `Option Base 1` exists | Same |
| Default parameter passing | **ByVal** | ByRef | ByVal |
| Array passed ByVal | Read-only in the callee | Copy | Callee can change elements |
| Parentheses around an argument | Never change ByRef to ByVal | `F (x)` passes x ByVal | No effect |
| `Dim` inside a block | Visible only in that block | Whole procedure | Block |
| Variants | None; every variable has a type | Default type | `Object` |
| Option Explicit | Always on, cannot be disabled | Optional | Optional |
| Integer | 32-bit (Long is 64-bit) | 16-bit | 32-bit |
| Errors | `Try/Catch/Finally` | `On Error GoTo` | `Try/Catch/Finally` |

## Names and declarations (Settled, 23 Oct 2026)

- Keywords are case-insensitive (`dim`, `Dim`, `DIM`). Everything else is
  case-sensitive: variables, Subs, Functions, controls, properties, events.
- The compiler warns when a Sub matches an event handler except for case
  (`Sub cmdok_Click` for control `cmdOK`), because it would never be called.
- `Dim x As Integer` is the normal form. Suffixes `%` (Integer), `&` (Long),
  `#` (Double), `$` (String) are optional shorthand; a suffix that
  contradicts `As` is an error.
- `Dim a, b As String` makes both String (VB.NET rule, not VB6).
- Reserved words cannot be used as names, including ones not yet used by
  the compiler (for example `Step`, `Until`, `Wend`, `ByRef`).

## Types and conversions (Settled, 23 Oct 2026)

- Integer (32-bit), Long (64-bit), Double, String (UTF-8, immutable,
  reference-counted), Boolean. Integer overflow is a runtime error.
- Option Strict is on: widening is implicit (Integer → Long → Double);
  narrowing needs `CInt`/`CLng`; String ↔ number is always explicit. The one
  exception is `&`, which turns numbers and Booleans into text.
- `Len`, `Mid$` and the other string functions count code points; `LenB`
  counts bytes.

### Numbers (Settled, 27 Oct 2026)

- A Double prints (with `&`, `CStr`, `Str$`) as the **shortest text that
  reads back as exactly the same value**: `0.1`, `3`, `0.30000000000000004`
  for 0.1 + 0.2, `1E+20`. Exponent form from 1E15 up and below 0.0001, as
  VB.NET. So `Val(CStr(x)) = x` always.
- `Str$` keeps VB's leading space for zero and positive numbers:
  `Str$(5)` is `" 5"`. `CStr(5)` and `"" & 5` are `"5"`.
- `CInt` and `CLng` round halves to even, as VB: `CInt(2.5)` = 2,
  `CInt(3.5)` = 4. `Fix` truncates towards zero, `Int` rounds down.
- Doubles never hold Infinity or NaN: division by zero is error 11, a
  result too large is error 6 (Overflow), and an undefined power such as
  `(-8) ^ 0.5` is error 5, as VB6.

### Numbers: literals, operators, conversions (Settled, 27 Oct 2026)

- A whole-number literal is an Integer if it fits, otherwise a Long (as
  VB.NET); `5&` forces Long, `5#` Double. `1.5`, `2E10` are Doubles.
  `&HFF` and `&O17` are hexadecimal and octal, always non-negative.
- Long → Double is an implicit widening (as VB.NET), although a Double
  holds whole numbers exactly only up to 2^53.
- `/` and `^` always give a Double. `\` needs Integer or Long operands (a
  Double is a compile error). `Mod` works on all three; its sign follows
  the left operand: `-7 Mod 3` = -1. Precedence, highest first: `^`,
  unary `-`, `* /`, `\`, `Mod`, `+ -`, `&` (so `-2 ^ 2` = -4, and `^`
  groups to the left: `2 ^ 3 ^ 2` = 64).
- `CInt`, `CLng`, `CDbl` accept a number or a String; from a String the
  whole text (less surrounding spaces) must be a number, else error 13
  (Type mismatch). `Val` is lenient as in VB: it ignores spaces, stops at
  the first stray character and returns 0 for no number. `CStr` also
  takes a Boolean ("True"/"False"); no conversion turns a Boolean into a
  number.
- Built-in function names (`CInt`, `CLng`, `CDbl`, `CStr`, `Str`/`Str$`,
  `Val`, `Fix`, `Int`) cannot be reused for variables or procedures.

## Sub and Function

### Parameters (Settled, 26 Oct 2026)

- Parameters are **ByVal by default**. Write `ByRef` to let the callee
  assign to the caller's variable. ByVal/ByRef appear only in the
  signature, never at the call site.
- A ByRef argument must be assignable: a variable, an array element or a
  Type field. A literal, expression or control property is a compile error.
- A ByVal parameter is a local copy; assigning to it inside the procedure
  does not affect the caller.
- A user Type passed ByVal is copied whole.
- An array passed ByVal is **read-only** in the callee: assigning to an
  element, `ReDim`, or passing it on ByRef is a compile error. No copy is
  made. Pass it ByRef to change it.
- A `Ref` passed ByVal copies the reference: the callee can change the
  record's fields but cannot point the caller's variable at another record.

```vb
Sub Swap(ByRef a As Integer, ByRef b As Integer)
    Dim t As Integer
    t = a: a = b: b = t
End Sub
```

### Returning a value (Settled, 27 Oct 2026)

- Both forms work, as in VB.NET: assign to the Function's name, or `Return`.
- Inside the Function, its bare name means the result variable (on either
  side of `=`). With parentheses, `Area(...)` is always a call, so recursion
  into a parameterless Function is written `F()`.
- A Function that never sets its result returns 0, `""` or False.
- In a Function, `Return` needs a value; in a Sub, `Return` has none and
  means `Exit Sub`.

```vb
Function Area(w As Integer, h As Integer) As Integer
    Area = w * h          ' or: Return w * h
End Function
```

### Calling (Settled, 27 Oct 2026)

- `Show a, b`, `Show(a, b)` and `Call Show(a, b)` all mean the same.
  Unlike VB6, extra parentheses never turn a ByRef argument into ByVal.
- A Function may be called as a statement; its result is discarded.
- Event handlers (`Sub cmdOK_Click()`) and `Sub Main` take no parameters.

## Loops

### Forms (Settled, 27 Oct 2026)

```vb
For i = 1 To 10 [Step n]  ...  Next [i]
Do [While cond | Until cond]  ...  Loop
Do  ...  Loop [While cond | Until cond]
While cond  ...  Wend          ' VB-DOS spelling
While cond  ...  End While     ' VB.NET spelling, same meaning
```

- `Exit For`, `Exit Do`, `Exit While`, `Exit Sub`, `Exit Function` leave the
  innermost construct of that kind, even from inside other loops.
- A Do loop tests at the top or the bottom, not both.

### For semantics (Settled, 27 Oct 2026)

- The loop variable is an existing numeric variable; `Next i` must name it
  if a name is given.
- Start, limit and Step are evaluated once, before the first pass; changing
  them inside the loop has no effect.
- Step 0 loops forever (as VB). The final increment can overflow:
  `For i = 1 To 2147483647` raises Overflow, as in VB.

## Scope (Settled, 27 Oct 2026)

- A `Dim` inside a block (If, For, Do, While) is visible only inside that
  block. The same name may be reused in sibling blocks. An inner Dim may
  not hide a local or parameter of an enclosing block.
- **Settled, 27 Oct 2026:** a `Dim` inside a loop starts fresh (0, `""`, False) on every
  pass. VB.NET instead keeps the value from the previous pass, which
  surprises people.

## Evaluation order (Settled, 27 Oct 2026)

- Expressions are evaluated strictly left to right, as in VB.NET, with the
  same result on every C compiler (C itself leaves the order unspecified).
  A Function that changes a variable does not change a value already read
  from it earlier in the same expression:

```vb
calls = 0
x = calls + Counted() + calls   ' 0 + 1 + 1 = 2, where Counted adds 1 to calls
```

- And/Or evaluate both sides (no short-circuit), as in VB6.

## Errors (Settled, 23 Oct 2026)

- `Try / Catch [When cond] / Finally / End Try`; several Catch blocks.
- `Throw number, description` raises; a bare `Throw` inside Catch
  rethrows. `Err` has Number, Description and Line.
- Finally runs on every way out: normal end, Exit, Return, an error (but
  not `End`: see below). An error inside Finally replaces the pending one.
- No GoTo into or out of a Try block.
- An error nobody catches restores the terminal, prints
  `file:line: runtime error N: message` and exits with status 1.

```vb
Try
    total = total \ count
Catch When Err.Number = 11
    total = 0                      ' division by zero
Catch
    MsgBox Err.Description        ' anything else
    Throw                          ' and pass it on
Finally
    Close #1                       ' always runs
End Try
```

### Errors (Settled, 27 Oct 2026)

- `Err.Raise Number [, Source [, Description]]` takes VB's arguments
  exactly, so VB code runs unchanged; `Err.Source` holds the source. (Its
  second argument is the Source, where Throw's is the description.)
- Out of memory ends the program even inside a Try: it cannot be caught.

### Error numbers (Settled, 27 Oct 2026)

TermForms is not VB-compatible, but error numbers are where VB habits and
VB documentation pay off most, so the numbering follows VB6 where it can:

- **Reuse VB6's number wherever the meaning is the same**, with VB6's
  wording character for character (`Catch When Err.Number = 53` means File
  not found, as it always has).
- **Never give a VB6 number a different meaning.** Numbers for things
  TermForms does not have (3 Return without GoSub, 20 Resume, 47-49 DLLs,
  96-98 COM objects) stay unused.
- **Ranges:** 1-899 the runtime (VB6's numbers, and VB6 also used 300-499
  for control and object errors, which forms will need); 900-999 the
  runtime's errors that VB6 has no number for; **1000 and up for programs'
  own errors** (Throw, Err.Raise). A program may raise any number,
  including the runtime's, for example to report "File not found" itself.
- A number added to the runtime gets its text in one table
  (`tf_error_text` in `runtime/src/core/tf_error.c`) and a row below.

| Number | Text | Raised by | Since |
| --- | --- | --- | --- |
| 5 | Invalid procedure call or argument | an undefined power such as `(-8) ^ 0.5`; `Throw 0` | now |
| 6 | Overflow | Integer and Long arithmetic, a Double too large, CInt/CLng out of range | now |
| 7 | Out of memory | running out of memory (always fatal, never caught) | now |
| 11 | Division by zero | `/`, `\`, `Mod` by zero | now |
| 13 | Type mismatch | CInt/CLng/CDbl of text that is not a number | now |
| 14 | Out of string space | joining strings into one longer than a string can hold (about 2 GB); catchable, the result is "" | now |
| 900 | Terminal error | the terminal cannot be opened or is lost (always fatal) | now |
| 9 | Subscript out of range | array index outside its bounds | arrays |
| 10 | This array is fixed or temporarily locked | ReDim of a fixed array | arrays |
| 91 | Object variable or With block variable not set | using a `Ref` that is Nothing | Types and Ref |
| 457 | This key is already associated with an element of this collection | adding a Dictionary key twice | Dictionary |
| 52-76 | VB6's file errors (53 File not found, 62 Input past end of file, 70 Permission denied, 76 Path not found, ...) | file I/O, mapped from the operating system's error | file I/O |
| 380 | Invalid property value | a control property set out of range | forms |

- **Not provided: 28 Out of stack space.** Portable C cannot detect
  running out of stack, so recursion that goes too deep crashes the
  program instead of raising an error.
- A string too long is error 14 and can be caught (Jim, 27 Oct 2026: 14 if
  the program can carry on, else 7). It is detected before anything is
  allocated, so it can; genuinely running out of memory stays 7 and fatal.
- Named constants for these numbers (`Catch When Err.Number =
  errFileNotFound`) come with `Const`/`Enum` (below).

### Errors: details (Settled, 27 Oct 2026)

- Catch takes no variable (VB.NET's `Catch ex As Exception` is an error):
  the error is in `Err`. `Err` keeps the last error after End Try, until
  the next error or `Err.Clear`. Err's properties are read-only.
- `Err.Line` is the line where the error happened, which may be inside a
  procedure the Try called.
- A statement that fails has no partial effect: in `x = a \ b` with b = 0,
  x keeps its old value.
- **`End` ends the program at once, even inside a Try: no Finally block
  runs**, as in VB. End means "stop now"; a program that needs cleanup
  before stopping should do it before the End (or leave the Try normally
  and End afterwards). This is the one exception to "Finally always runs".

```vb
Try
    Open "data.txt" For Output As #1
    End                            ' stops here: the Finally below does NOT run
Finally
    Close #1
End Try
```

### Errors (Proposed)

- `Throw number` alone uses VB's standard text for the number ("Division
  by zero", ...) or "Application-defined or object-defined error";
  `Throw 0` is error 5.
- Exit and Return cannot leave a Finally block (as VB.NET); leaving a loop
  that is wholly inside the Finally is allowed.
- A Try needs at least one Catch or a Finally. Catch clauses are tried in
  order; the first whose When is True (or that has no When) handles the
  error, and if none does, the error goes on after the Finally.

## Constants and enumerations (Settled, 27 Oct 2026)

`Const` and `Enum` are in v1 (Enum unless it proves complex). Most
programmers expect them, and they give error numbers and other magic
values names.

```vb
Const MAX_ITEMS = 100                 ' Integer, from the value
Const RATE = 0.075                    ' Double
Const TITLE = "Orders"                ' String
Const BIG As Long = 100               ' As (or a suffix) picks another type
Const LIMIT = MAX_ITEMS * 2           ' other Consts and operators

Enum Color                            ' Integer underneath
    Red                               ' 0
    Green                             ' 1
    Blue = 10
    Violet                            ' 11: counts on from the last value
End Enum

Dim c As Color
c = Color.Blue                        ' always qualified
n = c + 1                             ' a Color widens to Integer
c = CType(n, Color)                   ' Integer to Color needs CType
```

- **A Const's type comes from its value** by the literal rules (a whole
  number is an Integer if it fits, else a Long; a fraction is a Double;
  text is a String; True/False a Boolean), unless `As` or a suffix says
  otherwise, in which case the value must fit that type.
- **Enum members are always written qualified**: `Color.Red`, as VB.NET.
  Members may share names across Enums and with variables.
- **An Enum is its own type** under Option Strict: a `Color` variable takes
  only Color values. A Color widens implicitly to Integer (or Long), so
  arithmetic and comparisons with numbers work; Integer to Color needs
  `CType(n, Color)`.
- **As text an Enum value is its number**: `"" & Color.Red` is `"0"`, as
  VB6 and VB.NET's CStr.

### Constants and enumerations: details (Settled, 27 Oct 2026)

- A Const's value is a constant expression: literals, other Consts, Enum
  members, and the operators (`+ - * / \ Mod ^ & = <> < > <= >= Not And
  Or`). No function calls. It is worked out at compile time, and an
  overflow or division by zero there is a compile error.
- Const follows Dim's scope rules: module level, or inside a procedure
  with block scope. A Const cannot be assigned to. Module-level Consts and
  Enums are worked out in source order, so each can use only those above
  it.
- `Enum Name [As Integer | As Long]`; Integer by default. Members count
  from 0, or from a `= value` (a constant expression), adding 1 each time;
  two members may have the same value. Enums are declared at module level.
- `CType(n, Color)` does not check that n is one of Color's values (as
  VB.NET), so an Enum variable can hold any number of its underlying
  type.
- A For loop may run over an Enum variable (`For c = Color.Red To
  Color.Blue`); its Step, if any, is a plain number.

## References and Types (Settled, 23 Oct 2026)

- No user classes. `Type ... End Type` declares a record; `Dim c As
  Customer` is a value.
- `Ref Customer` is a reference that starts as `Nothing`; `New` allocates;
  `Dim c As New Customer` does both. No `Set` keyword. Compare with
  `Is Nothing` / `IsNot Nothing`. Reference cycles are never freed.

## Forms and events (Settled, 23 Oct 2026)

- A form's `Begin Form ... End` block of `Property = value` lines sits at the
  top of its `.bas` file. Width and Height include the border; Left and Top
  are measured inside it. Forms are fixed size and centred.
- `Sub <Control>_<Event>` is wired to that event at compile time.
  Form events use `Form_` whatever the form is called. `Form_Load` runs
  before the form is shown.
