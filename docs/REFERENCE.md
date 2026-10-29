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

### For Each (in v1: Settled, Jim, 27 Oct 2026; details Proposed)

```vb
For Each w In words             ' each element of an array, in turn
    s = s & w
Next w
For Each k In ages.Keys         ' a Dictionary's keys (Keys is an array)
```

- The loop variable is an existing variable that can hold the elements
  (Option Strict: Integer elements into a Long variable widen; Long into
  Integer is an error).
- **The variable gets a copy of each element**: changing it does not
  change the array (a Type element is copied; a Ref element shares its
  record, as Refs do).
- A multi-dimensional array is visited in storage order, the first index
  changing fastest: `grid(0,0), grid(1,0), grid(0,1), ...`.
- The array's path is evaluated once, as for With (`For Each x In
  list(i).Lines` keeps `i`, and a Ref on the way is kept). Each pass then
  looks at the array as it is: a ReDim inside the loop that shrinks it
  ends the loop early; one that grows it adds passes. Nothing is locked,
  so nothing needs undoing when the loop is left early.
- `Exit For` leaves it; `Next name` must name its variable.

### Select Case (Settled, Jim, 28 Oct 2026)

```vb
Select Case score
    Case 100
        grade = "perfect"
    Case 90 To 99, 85
        grade = "A"
    Case Is >= limit
        grade = "pass"
    Case Else
        grade = "fail"
End Select
```

- **VB6's Case forms**: a value, `a To b`, `Is <op> value` (`= <> < >
  <= >=`), several of them in one Case separated by commas, and
  `Case Else` last.
- **A Case may hold any expression.** The selector is evaluated once;
  the Cases are tried top to bottom and left to right, stopping at the
  first match, and what follows it is not evaluated.
- **The first matching Case runs**; the compiler warns when a constant
  Case can never be reached because an earlier constant one covers it.
- **Values match the selector as `=` does** (Option Strict): numbers
  widen, Strings compare exactly (case matters), an Enum selector takes
  its own members, and String and number do not mix.
- Details (Proposed): the selector may be a number, String, Boolean or
  Enum (not a Type, Ref or array). A comparison needs its `Is`
  (`Case > 5` is an error that says so, as VB6 would have written it
  `Case Is > 5`). `Case Else` must come last. Only blank lines and
  comments may come between `Select Case` and the first Case. The
  warning covers constant values, ranges and (on whole numbers) `Is`
  comparisons; a String only when the same text is repeated. A range
  whose first value is greater than its last also gets a warning: it
  never matches. There is no `Exit Select`; `Exit For` and the rest work
  from inside a Case as usual.

### Declarations with values, and in For (Settled, Jim, 28 Oct 2026)

```vb
Dim count As Integer = 5, name As String = "Ann"
Dim days() As String = {"Mon", "Tue", "Wed"}
Dim grid(,) As Integer = {{1, 2}, {3, 4}}
For i As Integer = 1 To 10 ... Next i
For Each w As String In words ... Next w
```

- **VB.NET's initialisers, each with its own type**: `Dim x As Integer
  = 5`, `Dim s$ = "a"`, several in one Dim. `Dim a, b As Integer = 5` is
  an error (as VB.NET), and there is no type inference (`Dim x = 5` stays
  an error).
- A local's initialiser runs where the Dim is, so inside a loop on every
  pass (the Dim starts fresh each pass, as settled).
- **A module-level or Public variable's initialiser may be any
  expression**, run once at program start, modules in project order,
  before Sub Main or the first Form_Load. An error there is reported as
  any uncaught error. A control read there gives its form-block value.
- **Array initialisers, as VB.NET**: a list in braces makes a dynamic
  array sized from it, nested braces for more dimensions (rows of equal
  length). A fixed size with a list (`Dim a(2) As Integer = {...}`) is
  an error.
- **`For i As Integer = ...` and `For Each w As String In ...`** declare
  the loop variable for the loop alone (gone after Next); it may not
  hide an enclosing local. The old form with an existing variable stays.
- Initialiser details (Proposed): the values in one Dim are given left
  to right, so a later one may use an earlier (`Dim a As Integer = 5, b
  As Long = a * 2`). A name with a suffix needs no As of its own (`Dim
  n%, m As Integer = 3` is fine). `{}` leaves the array empty (UBound of
  it is error 9, as for any empty array). A dynamic array may also be
  given another array (`Dim b() As Integer = a`, a copy). `As New`, a
  Dictionary and a fixed-size array take no value. Module variables are
  set after the forms are made and their event handlers wired, so a
  Function called there may use the controls.

### DoEvents (Settled, Jim, 28 Oct 2026)

- **As VB**: `DoEvents` redraws the screen, then handles every key,
  click and due Timer already waiting, without waiting for more, and
  returns. A long loop that calls it keeps the form live and its labels
  current.
- **Handlers may be re-entered**, as VB: a button clicked again while its
  Click handler is inside DoEvents runs that handler again; a program
  that must not allow it guards with a flag.
- **A statement only** (no VB6 return value).
- **Closing a form during DoEvents** runs its Form_Unload (which may
  cancel) and the code after DoEvents carries on; if no form is left
  shown, the program ends when that code returns to the event loop.
- Details (Proposed): **a form's `Visible` can be read** (True while it
  is shown; Show and Hide set it, so it is read-only), for code after a
  DoEvents to see whether the user closed it. DoEvents before any form
  is shown does nothing. **In a key script**, where every key is already
  waiting, DoEvents stops at `{IDLE}`; a script that runs out inside
  DoEvents ends the program, the screen dumped as it was (as it ends the
  event loop), so a test of a `Do: DoEvents: Loop` cannot hang.

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
| 5 | Invalid procedure call or argument | an undefined power such as `(-8) ^ 0.5`; `Throw 0`; a Dictionary key that is not there (read, Remove) | now |
| 6 | Overflow | Integer and Long arithmetic, a Double too large, CInt/CLng/CByte out of range | now |
| 7 | Out of memory | running out of memory (always fatal, never caught) | now |
| 11 | Division by zero | `/`, `\`, `Mod` by zero | now |
| 13 | Type mismatch | CInt/CLng/CDbl of text that is not a number | now |
| 14 | Out of string space | joining strings into one longer than a string can hold (about 2 GB); catchable, the result is "" | now |
| 900 | Terminal error | the terminal cannot be opened or is lost (always fatal) | now |
| 9 | Subscript out of range | an array index outside its bounds; UBound of an empty array; ReDim Preserve changing a dimension other than the last | now |
| 10 | This array is fixed or temporarily locked | ReDim of a fixed array passed ByRef; ReDim or Erase of an array whose element is being passed ByRef; adding to, removing from or clearing a Dictionary whose entry is being passed ByRef | now |
| 91 | Object variable or With block variable not set | using a `Ref` that is Nothing: its members, or copying from it | now |
| 457 | This key is already associated with an element of this collection | `d.Add` of a key that is there | now |
| 63 | Bad record number | a negative offset in Get, Put or Seek # | now |
| 58 | File already exists | `Name` onto a file that is there | now |
| 52-76 | VB6's file errors: 52 Bad file name or number, 53 File not found, 54 Bad file mode, 55 File already open, 57 Device I/O error, 61 Disk full, 62 Input past end of file, 67 Too many files, 70 Permission denied, 75 Path/File access error, 76 Path not found | files (see Text files) | now |
| 380 | Invalid property value | a control property set out of range (a CheckBox Value outside 0-2) | now |

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

### Errors: Throw, Finally and Catch order (Settled, 27 Oct 2026)

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

## Arrays (Settled, 27 Oct 2026)

```vb
Dim scores(9) As Integer          ' fixed: 10 elements, scores(0) to scores(9)
Dim grid(24, 79) As String        ' several dimensions: grid(row, column)
Dim names() As String             ' dynamic: empty until ReDim
Dim table(,) As Double            ' dynamic, two dimensions

ReDim names(n)                    ' sized when running
ReDim Preserve names(n + 10)      ' keeps the contents; only the last dimension may change
copy = names                      ' copies the elements: copy and names are separate
For i = 0 To UBound(names)        ' UBound(a [, dimension]); LBound is always 0
Erase names                       ' a dynamic array becomes empty
```

- **Always 0-based**, as decided on 23 Oct: `Dim a(10)` has 11 elements.
  There is no lower bound: `Dim a(1 To 10)` is a compile error.
- **Several dimensions** are allowed (up to 8); `ReDim Preserve` may change
  only the last one, as in VB. Elements are stored column-major, as VB.
- **Only a dynamic array can be resized.** `Dim a(10)` is fixed: ReDim of it
  is a compile error, and its size must be a constant (a Const will do).
  `Dim a()` (or `a(,)`, which also fixes the number of dimensions) is
  dynamic, sized by ReDim.
- **`b = a` copies the elements** (b must be dynamic, with the same element
  type and dimensions); changing one afterwards leaves the other alone.
- Every index is checked: one outside the array is error 9 (Subscript out
  of range), as is UBound of an empty array.
- Array parameters are written `values() As Integer` (or `(,)`), and take
  any array, fixed or dynamic, of that element type and dimensions. ByVal
  (the default) makes it read-only in the callee; ByRef lets the callee
  change or ReDim it (ReDim of a fixed array passed ByRef is error 10).
- **An element passed ByRef locks its array** for the call: ReDim or Erase
  of that array meanwhile is error 10 (This array is fixed or temporarily
  locked), so the callee can never write into memory a ReDim freed.
- `Erase` empties a dynamic array and resets a fixed one's elements. A Dim
  of an array inside a loop does the same on each pass.
- UBound and LBound return an Integer, so `For i = 0 To UBound(a)` works
  with an Integer i. Bounds in ReDim are Integers; indexes may be Integer
  or Long.
- Not yet: a Function returning an array (planned with `Split` in the
  string library step), and arrays of Types (with Types, next).

## References and Types (Settled, 23 Oct 2026)

- No user classes. `Type ... End Type` declares a record; `Dim c As
  Customer` is a value.
- `Ref Customer` is a reference that starts as `Nothing`; `New` allocates;
  `Dim c As New Customer` does both. No `Set` keyword. Compare with
  `Is Nothing` / `IsNot Nothing`. Reference cycles are never freed.

### Types (Settled, 27 Oct 2026)

```vb
Type Line
    Item As String
    Qty As Integer
End Type

Type Order
    Customer As String
    Lines() As Line          ' a dynamic array member
    Totals(3) As Double      ' a fixed array member
    Shipping As Address      ' another Type, held by value
End Type

Function NewOrder(who As String) As Order   ' a Function may return a Type
    NewOrder.Customer = who
    ReDim NewOrder.Lines(0)
End Function

Dim o As Order, copy As Order
o = NewOrder("Ann")
o.Lines(0).Qty = 2
copy = o                     ' a deep copy: copy.Lines is a separate array
```

- **A Type is a value**: assignment copies the whole record, strings,
  arrays and nested records included, so after `copy = o` the two share
  nothing. A ByVal Type parameter is the callee's own copy (it may change
  it freely); ByRef lets the callee change the caller's record.
- **Types may contain arrays**, fixed and dynamic; `ReDim o.Lines(n)`,
  `UBound(o.Lines)` and the rest work on them.
- **A Function may return a Type**; the caller gets its own copy.
- **Values and Refs convert by copying** (when Ref arrives): `r = c` makes r
  refer to a new copy of c; `c = r` copies the record r refers to (error 91
  if r is Nothing).
- **`With ... End With` is in v1**, for Types and Refs (details below).

### Refs: details (Proposed)

```vb
Type Node
    Value As Integer
    Next As Ref Node          ' a member may be named with a keyword
End Type

Dim head As Ref Node          ' Nothing until given a record
Dim n As New Node             ' a Ref given a new record when the Dim runs
n.Value = 1
n.Next = head
head = n                      ' head and n now refer to the same record
If head IsNot Nothing Then total = total + head.Value
```

- A Ref passed ByVal shares the record (the callee can change its members)
  but the callee cannot make the caller's variable refer to another
  record; ByRef can.
- Refs are compared with `Is` / `IsNot` (the same record, or Nothing);
  `=` on Refs is an error, as is `&`.
- A member of a record reached through a Ref may be assigned to wherever
  the Ref came from (even a Function's result): the record is shared.
- Using a Ref that is Nothing (its members, or copying from it) is error
  91, Object variable or With block variable not set.
- `Dim r As New Node` makes a new record each time the Dim runs (inside a
  loop, each pass).
- Keywords may name members (`Next`, `Step`, `Type`): after a dot, and in a
  Type block, nothing else could stand there, so it is not ambiguous.
- Freeing a record frees the records only it refers to, recursively: a
  very long chain (a linked list of millions) uses as much stack.

### With (Proposed, 27 Oct 2026)

```vb
With orders(i)
    .Customer = "Ann"
    With .Lines(0)                 ' nested: .Qty is orders(i).Lines(0).Qty
        .Qty = 2
    End With
    Bump .Total                    ' a .Member may be passed ByRef
End With
```

- **Targets:** a Type variable, element or member; any Ref (a variable,
  member, element, `New T` or a Function's result); a Function's Type
  result; a control or the form. Not a number, a String, a whole array
  or Nothing (compile errors).
- **Evaluated once, at the With** (as VB): a Ref, a Function call or
  `New` in the target is evaluated and kept, and so are the indexes of
  the elements after it, left to right. Changing `i`, or making `r` refer
  to another record, inside `With a(i)` / `With r` changes nothing; the
  record a Ref referred to stays alive until the With is done with it.
- **An element is looked up again at each use**, with the kept indexes
  (*Settled*, Jim, 27 Oct 2026).
  `ReDim Preserve` that keeps it is harmless; a ReDim that removes it
  makes the next use error 9. *VB6 differs*: it locks the array for the
  whole With (ReDim inside is error 10). Looking up again is as safe and
  needs nothing undone when the With is left by Exit, Return or an error.
- **A Function's Type result** is copied, and the copy cannot be changed
  (`.Name = ...`, ByRef, ReDim are compile errors), as for `F().Name`.
  Through a Ref result the record is shared and can be changed.
- **`.Member`** means the innermost With's target; outside any With it
  is a compile error. Inside, `.Member` works wherever `x.Member` would.
- **Spacing:** after a name, `Bump .Age` (a space before the dot) passes
  the With member `.Age` to `Bump`; `Bump.Age` is a member of `Bump`. VB
  reads them the same way.
- `With` cannot follow `Then` on a one-line If. Nothing needs undoing
  when the With is left early.

### Types: details (Proposed)

- Types are declared at module level. Members need a type (`As` or a
  suffix). A Type cannot contain itself by value, directly or through
  other Types (it would be infinitely large); a dynamic array of itself,
  or a Ref, is fine.
- Types cannot be compared with `=` or `<>` (compare members), joined
  with `&`, or converted with CType.
- A member of a Function's result can be read (`Make().Name`) but not
  assigned to.
- Copying into a record while one of its arrays is locked (an element
  of it is being passed ByRef) is error 10, as ReDim of that array would
  be.

## Dictionary (Settled, Jim, 27 Oct 2026)

```vb
Dim ages As Dictionary(Of Integer)    ' keys are always Strings
ages("Ann") = 30                      ' adds or replaces
ages.Add "Bo", 25                     ' error 457 if "Bo" is already there
If ages.Exists("Cy") Then n = ages("Cy")
For Each k In ages.Keys
    total = total + ages(k)
Next
```

- **Declared as `Dictionary(Of T)`**: the keys are Strings, the values all
  of type T.
- **A value, like an array**: ready to use at its Dim (no New); `d2 = d`
  copies it; passed ByVal it is read-only. To share one, put it in a Type
  and use a Ref.
- **Reading a key that is not there is error 5** (Invalid procedure call
  or argument), as VB6's Collection; `Exists` asks first.
- **Keys are case-sensitive** ("ann" and "Ann" are different keys).
- **Insertion order**: Keys, Items and For Each give the entries in the
  order they were first added.
- **Members, as VB6's Scripting.Dictionary**: `d(key)` (read, or assign to
  add or replace), `d.Add key, value` (457 if the key exists),
  `d.Exists(key)`, `d.Remove key`, `d.Count`, `d.Clear`, `d.Keys` (a
  String array) and `d.Items` (an array of T).

### Dictionary: details (Proposed)

- **Only `d(key) = value` (and Add) adds a key.** Any other use of an
  entry needs it to be there (error 5): reading it, changing a member of
  one (`d(k).Name = "x"`), passing it ByRef, a With on it.
- `Keys` and `Items` are **copies**, taken when evaluated: changing the
  Dictionary inside `For Each k In d.Keys` is safe. They are assigned to
  a dynamic array of their type (`Dim ks() As String: ks = d.Keys`) or
  used in For Each; nowhere else (`UBound(d.Keys)` is an error: use
  `d.Count`). An empty Dictionary's Keys has UBound -1, so `For i = 0 To
  UBound(ks)` runs no passes.
- While an entry is passed ByRef, adding a key, Remove and Clear are
  error 10 (as ReDim of an array whose element is passed ByRef);
  changing existing entries is allowed.
- A With on an entry (`With d(k)`) keeps the key and looks the entry up
  again at each use, as With does for an array element.
- Values may be any type but an array or a Dictionary (put either in a
  Type). A Dictionary may be a member of a Type; arrays of Dictionaries
  are not available (make an array of a Type that holds one). A
  Function cannot return a Dictionary (pass one ByRef to fill).
- ByVal: read-only, not copied (as arrays). Assignment `d2 = d` copies
  every entry; both must hold the same type.
- `""` is a key like any other. `Count` is an Integer.
- Remove takes time in proportion to the entries after the one removed
  (the order is kept); looking up and adding take about the same time
  however many entries there are.

## Strings and files (Settled, Jim, 27 Oct 2026)

- **String positions start at 1**, as VB (unlike arrays): `Mid$(s, 1, 3)`
  is the first three characters; `InStr` gives 1 for a match at the
  start and 0 for none.
- v1 has VB-DOS's string functions plus VB6's `Replace`, `Split`,
  `Join`, `InStrRev` and `StrReverse`.
- **`Print #` is simple**: `Print #1, a; b` writes a and b as text (as &
  would), nothing added; a trailing `;` leaves out the newline; a comma
  is an error (no 14-column zones, no space before positive numbers).
- **Files are written with LF line ends on every platform**; reading
  accepts LF and CRLF.
- **Binary files are wanted soon** (Jim): read a buffer of x bytes at
  offset y, the position advancing by x on each read, with the size or
  position settable in the read or by a separate statement. The byte
  type is to be decided.

### Files and bytes (Settled, Jim, 28 Oct 2026)

- **A `Byte` type** (0 to 255) holds binary data, one byte per element in
  Byte arrays.
- **Binary file offsets count from 0**: offset 0 is the first byte (VB
  counts Get/Seek positions from 1).
- **A read's size is the buffer's**: `Get` reads as many bytes as the
  Byte array holds, so a ReDim sets it for later reads; an optional count
  in the Get sets it for that read (the array resized to fit).
- v1 also has `Name ... As`, `FileCopy`, `FileLen`, `FileDateTime` and
  `Exists(path)` (not VB's; True if a file or folder is there). Not VB's
  `Input #` / `Write #`.

### Text files (Proposed)

```vb
n = FreeFile
Open "notes.txt" For Output As #n      ' or Append, Input (Binary: see below)
Print #n, "total="; total               ' items joined as & joins them
Print #n, "no newline yet";             ' a trailing ; leaves out the newline
Close #n
Open "notes.txt" For Input As #1
Do While Not EOF(1)
    Line Input #1, line
Loop
Close                                   ' every open file
```

- **File words are not reserved**: `Open`, `Close`, `Print`, `Line`,
  `Input`, `Output`, `Append`, `Binary` stay free as names (a Sub may be
  called Append). A statement is a file statement by its shape: `Print #`,
  `Line Input #`, `Close` before `#`, a number or the end of the line,
  and `Open` with `For`. `EOF`, `LOF`, `FreeFile` and `Seek` are built-in
  function names.
- File numbers run from 1 to 255; the `#` is optional (`As 1`, `Close 1`).
  `FreeFile` gives the lowest free number.
- Text is UTF-8. `Line Input` reads to LF, dropping a CR before it (so
  Windows files read the same); a byte that is not UTF-8 is read as U+FFFD.
  `Print #` writes LF.
- `EOF(n)` is True at the end of a file being read (and always for one
  being written). `LOF(n)` is its length in bytes; `Seek(n)` the offset of
  the next byte to be read or written, from 0.
- Errors: 52 a number outside 1-255 or not open; 54 the wrong mode
  (`Print #` to a file opened For Input); 55 opening a number in use; 62
  `Line Input` at the end; and from the system 53 (no such file, reading),
  76 (no such folder, writing), 70 (permission), 61 (disk full), 67 (too
  many files), 75 (other access errors). `Close` of a number not open does
  nothing, as VB.
- Files still open when the program ends are written out and closed.
- The program's current folder is where it was started; relative names
  are taken from there.

### Binary files (Proposed)

```vb
Dim buf() As Byte
Open "data.bin" For Binary As #1   ' read and write; made if it is not there
ReDim buf(511)
Get #1, 1024, buf                  ' 512 bytes from offset 1024 (offsets from 0)
Get #1, , buf                      ' the next 512: no offset, where the last ended
Get #1, , buf, 16                  ' 16 bytes; buf is resized to 16
Put #1, 0, buf                     ' write buf at offset 0 (or Put #1, , buf, n)
Seek #1, 0                         ' set the position; Seek(1) reads it
```

- The buffer is a Byte array with one dimension. Without a count, Get
  reads (and Put writes) as many bytes as it holds, so a ReDim sets the
  size for every later read; a count sets it for that one, and Get
  resizes the buffer to it (so it must be a dynamic array).
- Reading past the end is not an error: a dynamic buffer is shortened to
  the bytes read (UBound -1 for none), a fixed one has the rest set to 0.
  `EOF(n)` is True once the position is at or past the end.
- Put past the end makes the file longer; any gap is filled with zeros.
- Errors: 54 for Get/Put on a file not opened For Binary; 63 (Bad record
  number) for a negative offset; 5 for a negative count; 9 for a Put count
  larger than the buffer.

### The file system (Proposed)

| Statement or function | Does |
|---|---|
| `Kill pattern` | deletes the matching files (53 if none); `*` and `?` in the last part |
| `MkDir path`, `RmDir path` | makes / removes a folder (RmDir: only an empty one, else 75) |
| `ChDir path`, `CurDir$` | changes / gives the current folder |
| `Name old As new` | renames or moves a file or folder; never replaces one (58) |
| `FileCopy from, to` | copies a file, replacing `to` as VB does |
| `FileLen(path)`, `FileDateTime(path)` | the size in bytes (a Long); when it last changed, local time as "yyyy-mm-dd hh:mm:ss" |
| `Exists(path)` | True if a file or folder is there (not VB's) |
| `Dir$(pattern)`, `Dir$()` | the first matching file, then each next one, then "" |

- `Dir$` lists files, not folders, in sorted order (byte order of the
  UTF-8 names), so a program sees the same order on every system; a folder
  that cannot be read gives "". `Dir$()` before any `Dir$(pattern)` is
  error 5. Like `Kill`, it takes `*` and `?` in the last part only.
- Names are matched as the system does: exactly on Linux and macOS
  (macOS's file system itself usually ignores case), ignoring A-Z case on
  Windows. `/` separates folders everywhere; `\` too on Windows.
- `Name` is recognised by its shape (`Name ... As ...`), so `Name` stays
  free as a variable or member name.
- Errors: 53 no such file, 76 no such folder (in the path, or for MkDir,
  ChDir, RmDir), 58 Name onto a file that is there, 70 not permitted, 75
  any other refusal (a folder not empty, one already there).

### The system (Proposed)

| Function or statement | Gives or does |
|---|---|
| `Command$` | the program's arguments, joined by single spaces |
| `Environ$(name)` | an environment variable's value; "" if it is not set |
| `Shell(command)`, `Shell command` | runs command through the system's shell and waits; the function gives its exit code |
| `Now` | the local date and time, "yyyy-mm-dd hh:mm:ss" (until there is a Date type) |
| `Date$`, `Time$` | VB's "mm-dd-yyyy" and "hh:mm:ss" |
| `Timer` | the seconds since local midnight, a Double with the fraction |

- **`Command$` is the same on every platform**: the arguments as the
  program received them (Windows' quoting rules applied there, as the
  shell applies its own on Linux and macOS), joined by single spaces. VB
  gave the command line's tail as typed. An argument with spaces in it
  cannot be told from two: `CommandArgs()` gives them one by one.
- **`Shell` waits** for the command and gives its exit code (VB's Shell
  starts a program and returns at once with its task id). The command
  goes to `sh -c` on Linux and macOS and `cmd.exe /c` on Windows, so
  redirection and pipes work; a command that does not exist gives the
  shell's code (127 from sh, 9009 from cmd.exe), not an error.
  `Shell ""` is error 5. While it runs, a shown form hands the terminal
  back (the command may use it), and is redrawn after.
- The names `Command`, `Environ`, `Shell`, `Now`, `Date`, `Time` and
  `Timer` are built-in functions (the `$` is optional, as for the string
  functions), so they cannot name variables.

### Arguments and the program's path (Settled, Jim, 29 Oct 2026; not built yet)

```vb
For Each a As String In CommandArgs()       ' the arguments, one by one
    If a = "-v" Then verbose = True
Next
Dim cfg As String = App.Path & "/settings.txt"   ' beside the program
```

- **`CommandArgs()`** is a String array of the program's arguments, the
  program's name not included (as VB.NET's
  My.Application.CommandLineArgs); with none, its UBound is -1. They are
  as the operating system gives them: split by the shell on Linux and
  macOS, by Microsoft's C runtime rules on Windows. An argument with
  spaces stays one element. `Command$` stays as it is.
- **`App.Path`** is the folder the executable is in (no separator at the
  end), **`App.EXEName`** its name without `.exe`, and **`App.FullPath`**
  the executable's full path. They come from the operating system, not
  from how the program was started, so they are right however it was
  run; a symbolic link is followed to the real file.
- **No unmodified command line**: on Linux and macOS the shell removes
  quotes and expands wildcards before the program starts, and the
  original is not kept, so it could only be given on Windows. Leaving it
  out keeps programs the same everywhere.

### Console programs: Print, Line Input, End n (Settled, Jim, 29 Oct 2026; not built yet)

```vb
Sub Main()
    Dim name As String
    Line Input "Your name: "; name
    Print "Hello, "; name
    Dim count As Integer
    Do While Not EOF(0)                 ' the rest of standard input
        Dim line As String
        Line Input line
        count = count + 1
    Loop
    If count = 0 Then
        Print #0, "warning: no lines"   ' standard error
        End 2                           ' the exit code
    End If
End Sub
```

- **`Print`** writes to standard output with `Print #`'s rules (`;` joins
  with no spaces, a trailing `;` leaves off the newline, no comma
  zones); **`Print #0`** writes to standard error the same way (0 is
  never a FreeFile number). Lines end with LF, as files do. On Windows
  the console is switched to UTF-8 output.
- **While a form or dialog is on screen**, what Print and Print #0
  write is held and written, in order, when the terminal goes back to
  normal (the last form closing, End, an error, Shell), so a form
  program can log as it goes without spoiling the screen.
- **`Line Input [prompt;] var`** reads one line of standard input
  (typed, or from a pipe or file), without its line end (LF or CR LF);
  the prompt is written first, with no newline. **`EOF(0)`** is True at
  the end of standard input; reading past it is error 62 (Input past
  end of file), as for files. Line Input while a form or dialog is on
  screen is error 900 (Terminal error): the keys belong to the form.
  There is no `Input` with comma splitting (as for files).
- **`End n`** ends the program with exit code n (0 to 255; error 5
  otherwise, as Linux keeps only 8 bits). Plain `End` and the end of
  Sub Main give 0; an uncaught error gives 1, as now.

### Byte (Proposed)

```vb
Dim b As Byte, buf(511) As Byte
b = 65                       ' a constant 0 to 255 needs no conversion
n = b * 2                    ' Byte arithmetic is done as Integer: 130
b = CByte(n - 100)           ' storing an Integer back needs CByte (error 6 outside 0-255)
```

- A Byte widens to Integer, Long and Double. An Integer, Long or Double
  needs `CByte`, except a whole-number constant 0 to 255 (VB.NET's
  Option Strict rule). CByte rounds as CInt (half to even) and accepts a
  String, as CInt does.
- Arithmetic and negation on Bytes give Integers (as VB.NET); only
  storing into a Byte can overflow.
- **A For loop over a Byte steps past its end like any For loop**, so
  `For b = 0 To 255` and `For b = 3 To 0 Step -1` end in error 6 (the
  value after the last, 256 or -1, is not a Byte). Use an Integer loop
  variable for such ranges.
- As text (with &, CStr, Str$) a Byte is its number.

### String functions (Proposed)

| Function | Gives |
|---|---|
| `Len(s)`, `LenB(s)` | characters (code points); bytes of UTF-8 |
| `Mid$(s, start [, length])` | the characters from start (all the rest without length) |
| `Left$(s, n)`, `Right$(s, n)` | the first / last n characters |
| `InStr([start,] s, find)` | where find first starts at or after start (default 1), or 0 |
| `InStrRev(s, find [, start])` | where the last find starts that ends at or before start (default: the end), or 0 |
| `LCase$(s)`, `UCase$(s)` | ASCII letters changed; other characters as they are, for now |
| `LTrim$`, `RTrim$`, `Trim$` | spaces (not tabs) removed from the left, right, both |
| `Space$(n)`, `String$(n, s)` | n spaces; n copies of s's first character |
| `Chr$(code)`, `Asc(s)` | the character with that Unicode code point; the first character's |
| `Replace(s, find, with)` | every find replaced, left to right |
| `StrReverse(s)` | the characters in reverse order |
| `Split(s [, delimiter])` | the pieces between delimiters (default " "), as an array |
| `Join(a [, delimiter])` | a String array's elements with the delimiter (default " ") between |

- Each may be written with or without `$` (`Mid` = `Mid$`), except
  `String$`, since `String` alone is the type.
- *Settled (Jim, 27 Oct 2026): the four rules below on errors,
  comparison, case and the Mid$ statement; the rest of this section is
  Proposed.*
- **Bad arguments are error 5**: a start before 1, a negative length or
  count, `Asc("")`, `String$(n, "")`, `Chr$` of a number that is not a
  character. A start past the end gives "" (Mid$) or 0 (InStr). A result
  longer than a string can hold is error 14.
- `InStr(s, "")` gives start (VB); `Replace(s, "", x)` gives s;
  `Split("")` gives no elements (UBound -1); `Split(s, "")` gives s.
- **Split gives an array**, used as d.Keys is: assigned to a dynamic
  String array, or in For Each. `Join` takes a String array variable.
- Comparison is exact (case-sensitive, byte for byte): VB's optional
  compare argument (vbTextCompare) is not provided.
- **Full Unicode case** (é to É) is not built: Phase 4's tables give
  character widths only. LCase$/UCase$ change only A-Z and a-z.
- **The `Mid$` statement** (wanted, Jim; built 28 Oct 2026):
  `Mid$(s, start [, length]) = text` replaces characters of s in place,
  as VB: as many as the text has, the length allows and s has room for
  from start, so s never grows or shrinks; counted in code points. s is a
  String variable, element or field (not a ByVal array's element). A
  start outside 1 to Len(s), or a negative length, is error 5 (so an
  empty s always is). `Mid` without `$` works too.

## Declare: calling C (Settled, Jim, 28 Oct 2026)

```vb
Declare Function add_tax Lib "utils.c" (ByVal cents As Integer) As Integer
Declare Function sqlite3_libversion Lib "sqlite3" () As String
Declare Function clock Lib "" () As Long
Declare Sub fill_name Lib "utils.c" (ByRef buffer As String)
```

- **Lib says where the function is.** A name ending in `.c` is a C file
  of the program (a path relative to the `.bas` file): tfc compiles it
  with the program, once however many Declares name it, and it
  cross-compiles with the rest. Any other name is a library to link
  (`-lsqlite3`; `sqlite3.lib` with MSVC). `Lib ""` is the C library
  itself. Libraries are joined in at build time, not loaded at run time
  as VB6's DLLs were; a cross-build needs the library built for the
  target.
- **Parameters are ByVal unless marked ByRef**, as for every Sub (not
  VB6's ByRef default for Declares). ByVal passes the value; ByRef a
  pointer to it.
- **tfc writes the C prototype from the Declare**; no header is needed.
  Integer is `int32_t`, Long `int64_t`, Double `double`, Byte `uint8_t`,
  Boolean `int` (0 or 1); ByRef adds a `*`. The C function must be
  declared with exactly those types (the guide warns that C's `long` is
  64-bit on Linux and macOS but 32-bit on Windows).
- **Strings**: a ByVal String reaches C as `const char *`, UTF-8 ending in
  NUL, valid for the call. A String result is a `const char *` the C side
  keeps owning: TermForms copies it (NULL is `""`). **A ByRef String is a
  writable buffer**: C gets a `char *` to a copy of its bytes (LenB plus a
  NUL), and after the call the String is the bytes up to the first NUL
  (VB6's `Space$(260)` idiom).
- **Arrays and Types**: an element passed ByRef (`buffer(0)`) gives C a
  pointer to it, with the rest of the array after it (arrays of numbers
  or Bytes). A Type whose members are all numbers, passed ByRef, gives a
  pointer to a C struct with the same members in the same order. No
  Strings, Refs, arrays or Dictionaries inside such a Type, and no whole
  arrays, Refs, Dictionaries or controls as arguments.
- **No callbacks in v1** (VB6's AddressOf): after v1.
- **C types whose size differs by platform** (Jim, 28 Oct 2026): a
  Declare may use `CLong`, `CULong` and `CSizeT` for C's `long`,
  `unsigned long` and `size_t`. In BASIC each is a Long; the prototype
  names the C type, so every platform's C compiler uses its own size, and
  a value that does not fit there (a `long` is 32-bit on Windows; an
  unsigned one takes no negatives) is error 6, Overflow. They exist only
  in Declares. (Not to be confused with CLng, the conversion.)
- **No conditional compilation in v1** (VB6's `#If` / `#Const`): after
  v1; CLong and Platform cover the common needs.

### Platform (Settled, Jim, 28 Oct 2026)

- `Platform` is a built-in read-only String: `"Windows"`, `"macOS"` or
  `"Linux"`, for the machine the program was built for. The C compiler
  sets it, so a program cross-built on a Mac for the Linux server says
  "Linux". It is not a Const (tfc does not know the target), so it
  cannot go in a Const or an Enum. For choosing Shell commands, paths
  and the like.

### Declare: details (Proposed)

- `Declare Function name Lib "..." [Alias "cname"] ([params]) As type`,
  and `Declare Sub` for a C function returning void. Alias gives the C
  name when it differs from the BASIC one (a C name that is a reserved
  word in BASIC, or clashes with another). Without Alias the C name is
  the BASIC name, exactly (it is not mangled).
- Declares are at module level; Public unless marked Private, as Subs.
  Called like any Sub or Function, with the same three call forms.
- A C function cannot raise a TermForms error: a call of one is never
  checked for one. What C does with bad input is C's business.
- A Declare names one Lib; two Declares naming the same C function with
  different types are an error.
- A program's `.c` files are compiled first, each on its own, with the
  same C compiler (and target) and tfc's include path (for
  `termforms/tf_runtime.h`), but not tfc's `-std=c99` or `-w`: they are
  the programmer's C, so its warnings and errors are shown as the C
  compiler gives them, and a failure stops the build there.
- When a program with Declares fails to link (a C function in no Lib, a
  library not installed, or not built for the target), the C compiler's
  messages are shown with that explanation, rather than tfc's "internal
  error" (which a program without Declares cannot cause).
- A C name may not be one C keeps for itself, nor start with a prefix
  tfc's own C uses (`tf_`, `c_`, `g_`, `v_`, `p_`, `t_`, `r_`, `a_`,
  `s_`), nor be `main`: it would clash. The C function must then be
  renamed (an Alias only helps when the BASIC name is the problem).
- `Platform` is `"Other"` when built for none of the three.

## Modules and projects (Settled, Jim, 28 Oct 2026)

- **A project file, `.vbp`, in VB6's style**: `Key=Value` lines, one file
  per line (`Form=`, `Module=`), `Name=` and `Startup=`; an unknown key is
  an error.
- **VB's visibility defaults**: Subs, Functions, Types and Enums are
  Public unless marked `Private`; module-level `Dim` and `Const` are
  Private unless declared `Public` (`Public total As Long`).
- **One form plus any number of code modules** for now; programs of
  several forms come with Phase 4's Show/Hide and modal stack.
- **`tfc a.bas b.bas` builds the files as one program** without a
  project file; a `.vbp` is for a name or settings.

### Modules (Proposed)

```vb
' stock.bas                          ' main.bas (the form's module)
Public Type Item                     Sub Form_Load()
    Name As String                       AddStock "bolt", 5
End Type                                 t1.Text = total & stock.Label()
Public total As Integer              End Sub
Sub AddStock(n As String, q As Integer)
    total = total + q
End Sub
```

- A module is a `.bas` file; **its name is the file's name without
  `.bas`**, so with several modules each file name must be a BASIC name.
- **Module names ignore case** (settled, Jim, 28 Oct 2026): `Stock.x`,
  `stock.x` and `STOCK.x` name the same module, as Windows and macOS find
  `Stock.bas` as `stock.bas`; two files whose names differ only in case
  cannot be modules of one program. This is the one exception to names
  being case-sensitive: a module's name comes from a file name, whose
  case is not dependable.
- Each module sees its own names and every other module's Public ones.
  **`Module.Name`** names one explicitly (a variable, Const, Function,
  array element, or a Sub called as a statement); a name Public in two
  other modules must be written that way (it is an error otherwise), and
  a module's own name wins over another's Public one.
- A name that is a module's name, in any case, cannot also be declared
  (with several modules).
- **The form is known everywhere**; another module reaches its controls
  through it: `frmMain.txtName.Text = "x"`.
- The program starts at its form, or at `Sub Main` (one in the program).
- Module-level variables are set up before anything runs, module by
  module in the order given; a Const may use Public Consts of modules
  given before its own.
- Errors (at compile time and at run time) name the file they are in.
- Types and Enums are named bare (`Dim p As Item`); one Public in two
  modules is ambiguous and must be made Private in one, for now.

### Project files (Proposed)

```
' inventory.vbp
Name=Inventory
Startup=frmMain
Form=frmMain.bas
Module=lib/stock.bas
```

- `tfc inventory.vbp` builds the program. `Form=` and `Module=` list its
  files in order (the order module variables are set up in), relative to
  the `.vbp`'s folder; the program is named by `Name=` (else after the
  `.vbp`) and built in that folder.
- Keys are Name, Startup, Form and Module, in any case; values are
  trimmed; blank lines and lines starting with `'` are ignored.
- What the file says must hold: a `Form=` file has a `Begin Form` block
  and a `Module=` file has none, and `Startup=` (if given) names the form
  or `Sub Main`, where the program does start. An unknown key, a repeated
  Name or Startup, or no files at all is an error, reported with the
  `.vbp`'s line.
- A `.vbp` is given on its own (not with `.bas` files).

## Phase 4: controls, forms and keys (Settled, Jim, 28 Oct 2026)

- **Order of work**: Label, CheckBox, OptionButton, Frame; ListBox;
  Timer; MsgBox/InputBox and the modal stack; several forms; menus; the
  TextBox edit buffer; mouse; Unicode widths.
- **Frames contain controls**, nested in the form block (`Begin Frame` ...
  `Begin OptionButton` ... `End` ... `End`): their Left and Top are inside
  the Frame, they move with it, and OptionButtons in one Frame are a group
  of their own (as VB).
- **VB's named constants are built in** (`vbYesNo`, `vbQuestion`, `vbYes`,
  `vbKeyReturn`, ...), reserved as built-in names.
- **Keys**: a `Default` button is pressed by Enter and a `Cancel` button
  by Esc anywhere on the form; arrows move within an OptionButton group
  and a ListBox, Tab between controls; `&` in a Caption makes an access
  key (Alt+letter); Space toggles a CheckBox, selects an OptionButton and
  presses a button.
- **Several forms are a stack**: `frmX.Show vbModal` waits until frmX
  closes; `frmX.Show` puts it on top and returns; only the top form takes
  keys, the ones below stay visible; Hide/Unload takes it off.
- **Events in v1**: Click, DblClick, Change; GotFocus, LostFocus;
  KeyPress (KeyAscii, which may be changed or set to 0), KeyDown;
  Form_Unload (Cancel), Form_Activate.
- **ListBox selects one or several items** (VB's MultiSelect and
  Selected(i) too).
- **The mouse is on by default**: clicks and the wheel go to the form;
  Shift+drag still selects text in most terminals; an environment variable
  turns it off.
- **Ctrl+F4 closes the top form** (Alt+F4 too where the terminal passes it
  on), raising Form_Unload; closing the last ends the program. Ctrl+Q no
  longer closes anything.
- **Unicode width tables** are generated from Python's unicodedata by a
  script in tools/ (no download); the Unicode notice ships in
  runtime/THIRD_PARTY_NOTICES.

### The Option-key hint (Settled, Jim, 28 Oct 2026; details Proposed)

- Out of the box macOS terminals type Option+letter as a character (˜,
  ¥, ∫) instead of sending Alt, so no access key can arrive. **When one
  of those characters comes where typing means nothing, a MsgBox shows,
  once a run, how to set the terminal** (iTerm2, Terminal.app, Ghostty)
  and that F10 or Ctrl+B opens the menus meanwhile. The key itself does
  nothing.
- Details (Proposed): "where typing means nothing" is the focus on
  anything but a TextBox (a button, a CheckBox, a list), or the menus
  open; in a TextBox the character is typed, as it may be meant. The
  characters are the plain symbols of the US Option layer (˜ ¥ ∫ ∂ ƒ ©
  ˙ ∆ ˚ ¬ µ π ® † √ ∑ ≈ Ω ¨ ˆ ´); å, ç, ø, ß and œ are left out, being
  letters on European keyboards. It happens on every platform, as the
  terminal may be a Mac's over SSH.

### Controls (Settled, Jim, 28 Oct 2026)

| Control | Shows as | Properties beyond Left, Top, Width, Height, Enabled, Visible | Events |
|---|---|---|---|
| Label | its Caption (words wrapped if Height > 1) | Caption | Click |
| TextBox | an edit field | Text, SelStart, SelLength, SelText, MaxLength, Locked, PasswordChar | Change |
| CommandButton | `< Caption >` | Caption, Default, Cancel | Click |
| CheckBox | `[X] Caption` (`[ ]`, `[-]` for 2) | Caption, Value (0, 1, 2) | Click |
| OptionButton | `(•) Caption` | Caption, Value (Boolean) | Click |
| Frame | a box with its Caption in the top line | Caption | (none) |
| ListBox | a box of items, scrolling (↑ ↓ in its border when there is more) | Text (the current item), ListIndex, ListCount (read only), MultiSelect (0, 1, 2), Sorted, SelCount (read only), TopIndex; `List(i)`, `Selected(i)` | Click, DblClick |

- Setting `Value` in code fires Click when it changes (as VB); a CheckBox
  Value outside 0-2 is error 380. Selecting an OptionButton clears the
  others of its group.
- A disabled control is drawn dim and skipped by Tab; a hidden one is not
  drawn. A Frame's Enabled and Visible apply to everything in it.
- Only one Default and one Cancel button per form: setting one clears
  the other's.
- A Label's access key moves the focus to the next control in tab order
  (put a Label just before the control it names, as in VB).
- Enter and Esc in a TextBox go to the Default and Cancel buttons.
- Tab order is the order the controls are written in the form block.
- `Ctrl+Q` no longer closes a form (Ctrl+F4 or Alt+F4 does).
- A ListBox's statements are `AddItem item [, index]`, `RemoveItem
  index` and `Clear`; `List(i)` reads or sets an item's text and
  `Selected(i)` its selection. An index out of range is error 5 (setting
  ListIndex out of range, 380). With Sorted, AddItem without an index puts
  the item in order (A-Z case ignored); with an index, there.
- ListBox keys: arrows, PgUp/PgDn, Home/End move the current item; a
  letter goes to the next item starting with it; Enter is DblClick when
  the program has a DblClick handler (else the Default button).
  MultiSelect 1: moving does not select, Space toggles the current item.
  MultiSelect 2: moving selects just the current item, Space adds or
  removes it (VB's Shift+arrow extension comes when Shift on arrows is
  read).
- Changing the selection (by key or in code) fires Click; removing the
  current item leaves no current item (ListIndex -1), as VB.
- **A Timer** is never shown: it raises its Timer event every Interval
  milliseconds while Enabled (Interval 0 stops it). It counts from when the
  form is shown, and again from whenever Interval or Enabled is set. One
  that falls behind (a long handler) fires once and counts on from then,
  rather than catching up in a burst. Timers run only while the form waits
  for keys (not while code runs). In a key script, `{WAIT n}` makes n ms
  pass, so tests of Timers are exact.

### MsgBox, InputBox and VB's constants (Settled, Jim, 28 Oct 2026)

```vb
If MsgBox("Save changes?", vbYesNoCancel + vbQuestion, "Editor") = vbYes Then Save
MsgBox "Done"                                  ' as a statement: the answer unused
name = InputBox("Your name:", "Hello", "Ann")  ' "" if cancelled
```

- `MsgBox(prompt [, buttons [, title]])` shows a dialog over the forms
  and waits: `buttons` is VB's sum of a button set (vbOKOnly, vbOKCancel,
  vbAbortRetryIgnore, vbYesNoCancel, vbYesNo, vbRetryCancel), an icon
  (vbCritical, vbQuestion, vbExclamation, vbInformation, drawn as `(X)`,
  `(?)`, `(!)`, `(i)`) and a default button (vbDefaultButton2, 3); it
  returns vbOK ... vbNo. The prompt's lines break at vbCrLf / vbLf and
  wrap at word ends.
- `InputBox(prompt [, title [, default]])` returns the text typed, or ""
  if cancelled. The focus starts in its TextBox; a MsgBox's on its default
  button.
- With no title, a dialog's title is the program's name (as VB uses the
  project's).
- Esc answers Cancel when the box has a Cancel button, OK when it is an
  OK box, and does nothing on Yes/No and Abort/Retry/Ignore; Ctrl+F4 the
  same. Buttons have access keys (&Yes, &No, &Abort, &Retry, &Ignore).
- A dialog may come before any form is shown (in Form_Load, or a program
  of Sub Main): it then opens the terminal itself. Timers of the forms
  under a dialog keep running.
- **VB's named constants** are built in and their names reserved: those
  above, vbOK ... vbNo, vbUnchecked / vbChecked / vbGrayed, vbModal /
  vbModeless, vbCr, vbLf, vbCrLf, vbTab, vbNullString, and **vbNewLine,
  which is vbLf** (TermForms writes lines with LF; VB's is CR LF).
- When a key script ends with a dialog shown, the screen is dumped as it
  is then, the dialog included.

### Several forms and the events (Settled, Jim, 28 Oct 2026)

```vb
Sub cmdOptions_Click()
    frmOptions.Show vbModal          ' waits until frmOptions is hidden or unloaded
End Sub
Sub Form_Unload(ByRef Cancel As Integer)
    If MsgBox("Quit?", vbYesNo) = vbNo Then Cancel = 1
End Sub
Sub txtCode_KeyPress(ByRef KeyAscii As Integer)
    If KeyAscii < Asc("0") Or KeyAscii > Asc("9") Then KeyAscii = 0  ' digits only
End Sub
```

- Each form is in its own module; form names are the whole program's
  (two alike is an error), and other modules reach a form's controls
  through it (`frmMain.txtName`). `Me` is the form of the module it is
  written in.
- `frm.Show` puts the form on top (loading it first); `frm.Show vbModal`
  also waits until it is hidden or unloaded. `frm.Hide` takes it off.
  `Load frm` runs its Form_Load without showing it; `Unload frm` runs
  Form_Unload (whose Cancel may refuse) and hides it, so the next Show
  loads it again.
- **An unloaded form is made afresh when it is next loaded**, as VB: its
  own and its controls' properties go back to the form block's settings
  (and the runtime's defaults), ListBoxes are emptied, the focus and the
  menus start again, and then Form_Load runs. No events fire while it is
  remade. It happens at the next Load, not at Unload, so until then the
  program can still read what the user left in it (VB would load it
  afresh on that read).
- **Form_Load runs when the form is first shown or loaded**, not when the
  program starts.
- **The program starts** at the project's Startup, else at Sub Main if
  there is one, else at the first form. After Sub Main, the forms it
  showed run until the last is closed; the program ends when no form is
  shown.
- **A handler that may change its argument declares it ByRef**, since
  parameters are ByVal by default (a ByVal one would silently lose the
  change): `Form_Unload(ByRef Cancel As Integer)`, `KeyPress(ByRef
  KeyAscii As Integer)`, `KeyDown(ByRef KeyCode As Integer, Shift As
  Integer)`. Any other signature is a compile error that shows the right
  one.
- KeyDown comes first, with VB's codes (vbKeyA, vbKeyReturn, vbKeyF2, ...)
  and Shift's bits (vbShiftMask, vbCtrlMask, vbAltMask); then KeyPress for
  a character, Enter (13), Backspace (8), Esc (27) or Ctrl+letter (1-26).
  Setting KeyCode or KeyAscii to 0 throws the key away; KeyPress may
  change a character to another.
- A form coming to the top (shown, or uncovered) gets Activate, then its
  first control the focus (GotFocus), as VB. **A MsgBox or InputBox
  closing does not activate the form under it** (Jim, 28 Oct 2026): as
  in VB6 it is not a form to the program, so Activate fires only when
  one of the program's own forms comes to the top, and the focus stays
  where it was. Moving the focus raises
  LostFocus on one control and GotFocus on the next.

### Menus (Settled, Jim, 28 Oct 2026)

The handover settled F10 and Alt+letter, and a Ctrl alternative for every
function key; the rest follows VB-DOS and VB6's `.frm` form.

```vb
Begin Menu mnuFile
    Caption = "&File"
    Begin Menu mnuSave
        Caption = "&Save"
        Shortcut = "Ctrl+S"
    End
End
```

- **Two levels, as VB-DOS**: a `Begin Menu` in the form block is on the
  menu bar; one inside it is an item of its drop-down. A third level, a
  Menu in a Frame, or another control in a Menu is a compile error.
- A Menu has Caption, Enabled, Visible, Checked, Shortcut and the Click
  event. A Caption of `-` is a separator line. Only items have Checked
  and Shortcut (a bar menu with either is a compile error).
- **The menu bar is the form's first row inside the border**, and the
  client area starts below it: with a menu bar, Top 0 is the row under
  the bar (as VB, whose menu is outside the client area).
- **F10 goes to the menu bar; so does Ctrl+B**, the Ctrl alternative the
  handover asks for (macOS and many keyboards take the function keys).
  Ctrl+B is taken only on forms with a menu bar.
- **Alt+letter opens the menu** with that access key, ahead of a
  control with the same one (as VB).
- On the bar: Left/Right move round the ends, Down or Enter opens, a
  letter opens that menu, Esc (or F10/Ctrl+B) leaves. In a drop-down:
  Up/Down move (passing over separators and disabled items), Enter or an
  item's letter chooses it, Left/Right open the next menu, Esc goes back
  to the bar. Other keys are ignored while the menus are in use, except
  Ctrl+F4 (Alt+F4), which leaves them and closes the form.
- **Opening a menu fires its Click** first (VB's chance to set the items'
  Checked and Enabled); choosing an item closes the menus, then fires the
  item's Click. A bar menu with no items is chosen like an item.
  Choosing never changes Checked: the program does (as VB).
- **Shortcut** is text: `Ctrl+letter`, `F1`-`F12` with any of `Shift+`,
  `Ctrl+`, `Alt+` before it, or `Del`, case ignored, shown as Windows
  writes it (`ctrl+s` shows as Ctrl+S). Not allowed: Ctrl+H, Ctrl+I,
  Ctrl+M (a terminal sends Backspace, Tab, Enter), Ctrl+B, F10, Ctrl+F4,
  Alt+F4 — a compile error in the form block, error 380 when set in code.
  A Shortcut works while its form is on top and the item and its menu
  are enabled and visible, before the focused control sees the key (no
  KeyDown).
- The drop-down is drawn over the form and may reach past its edge.

### TextBox editing (Settled, Jim, 28 Oct 2026)

The TextBox sits on an edit buffer of its own (runtime `tf_edit.c`), so
the multi-line TextBox and the ComboBox after v1 edit the same way. Keys
and properties follow a Windows edit control, as VB's TextBox does.

- **Keys**: Left/Right, Home/End; Shift with them selects; Ctrl+Left and
  Ctrl+Right move by words (Shift too); typing replaces the selection;
  Backspace and Del delete it; Ctrl+A selects all; Ctrl+X, Ctrl+C and
  Ctrl+V cut, copy and paste; Ctrl+Z undoes the last change and again
  redoes it (a run of typing is one change). Left or Right with a
  selection goes to its start or end. There is no overwrite mode (VB's
  TextBox has none).
- **The clipboard is the program's own**: what Ctrl+C copies, Ctrl+V
  pastes, within the program. The terminal's own copy and paste work as
  ever (Shift+drag, and its paste key, which types the text in).
- A word is letters, digits and `_`; anything beyond ASCII counts as a
  letter.
- **SelStart, SelLength** are in code points (as Len); setting them
  clamps to the text, and a negative value is error 380. Setting
  SelLength selects from SelStart with the caret at the end. **SelText**
  is the selected text; setting it replaces the selection.
- **MaxLength** (code points, 0 no limit) limits what the user types or
  pastes, not what the program sets. **Locked** lets the user move,
  select and copy, but not change the text. **PasswordChar** (its first
  character; "" for none) is shown for each character, and Cut and Copy
  give nothing out.
- **Change fires when the program changes the text too** (setting Text
  or SelText), as VB; not when it is set to what it already is.
- Setting Text puts the caret at the end, with nothing selected, and
  forgets the undo. The selection is shown only while the box has the
  focus (as Windows). The focus arriving does not select the text (as
  VB; set SelStart and SelLength in GotFocus for that).
- **A paste arrives as text** (the terminal's bracketed-paste mode,
  2004): its line breaks and tabs become spaces and other control
  characters are dropped, so a paste never presses Enter (a Default
  button) or Tab. Each character goes to the focused control as if typed
  (KeyPress sees it). A terminal without the mode sends a paste as keys.
- A long paste is no longer cut at 64 bytes: the input reads only what
  its decoder has room for.

### The mouse (Settled, Jim, 28 Oct 2026)

Settled: on by default, clicks and the wheel go to the form, Shift+drag
still selects text in most terminals, an environment variable turns it
off. The rest:

- **`TERMFORMS_MOUSE=0` turns it off**; anything else, or unset, leaves
  it on. It uses xterm's SGR reports (modes 1002 and 1006); the terminal
  layer turns them off whenever it restores the terminal (exit, error,
  SIGHUP, Shell).
- **The left button and the wheel only**; the others are ignored. Only
  the top form takes the mouse (as it alone takes keys).
- **A press focuses** the control under it (if it can take the focus).
  Buttons, CheckBoxes, OptionButtons and Labels **act on the release**,
  if it is over the same control (as VB: pressing and moving away is no
  click).
- **TextBox**: a click places the caret, a drag selects, a double-click
  selects the word.
- **ListBox**: a click chooses the item as the keys do (MultiSelect 1
  toggles it; MultiSelect 2 chooses just it, Ctrl+click toggles it); a
  double-click on the current item is DblClick; the wheel scrolls three
  rows a notch; a click on the ↑ or ↓ in its border scrolls one.
- **Menus**: a click on a bar menu opens it (again: closes it); a press
  on an item highlights it and the release there chooses it; a press
  anywhere else closes the menus and does nothing more (as Windows).
- **A double-click** is two presses on one cell within 500 ms (in a key
  script, with no `{WAIT}` between them); a third press starts again.
- Key scripts: `{CLICK x,y}`, `{DBLCLICK x,y}`, `{PRESS x,y}`,
  `{DRAG x,y}`, `{RELEASE x,y}`, `{WHEELUP x,y}`, `{WHEELDOWN x,y}`,
  with cells counted from 0,0 at the screen's top left.
- No MouseDown/MouseUp/MouseMove events in v1 (not in the settled list).

### Character widths (Settled, Jim, 28 Oct 2026)

Settled: the tables come from Python's unicodedata by a script in
tools/, and the Unicode notice ships in runtime/THIRD_PARTY_NOTICES.

- **2 cells**: East_Asian_Width W or F (CJK, Hangul syllables, fullwidth
  forms, and the emoji shown as emoji by default), and the unassigned
  code points of the ideograph blocks.
- **0 cells, not drawn**: combining marks (Mn, Me), format characters
  (Cf, except the soft hyphen and the prepended concatenation marks),
  Hangul medial vowels and final consonants, and controls. A cell holds
  one character, so a combining accent is left out (precomposed text,
  the norm, shows in full); leaving them out keeps every column where the
  terminal puts it.
- **1 cell**: everything else, the ambiguous width (A) included, as
  terminals draw it outside a CJK locale.
- Text is measured in cells wherever it is laid out: captions, the
  title, Label wrapping, dialogs, menus, ListBox items, the TextBox
  (which scrolls by whole characters). A wide character that would cross
  the edge of its space is left out. `Len`, `Mid$`, SelStart and the rest
  still count code points.
- `python3 tools/gen_width_tables.py` remakes the tables; run it with a
  newer Python for a newer Unicode, and commit the result.

## Forms and events (Settled, 23 Oct 2026)

- A form's `Begin Form ... End` block of `Property = value` lines sits at the
  top of its `.bas` file. Width and Height include the border; Left and Top
  are measured inside it. Forms are fixed size and centred.
- `Sub <Control>_<Event>` is wired to that event at compile time.
  Form events use `Form_` whatever the form is called. `Form_Load` runs
  before the form is shown.
