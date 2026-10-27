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

### For semantics (Proposed)

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
- **Proposed:** a `Dim` inside a loop starts fresh (0, `""`, False) on every
  pass. VB.NET instead keeps the value from the previous pass, which
  surprises people.

## Evaluation order (Proposed)

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
- `Throw number, description` raises (`Err.Raise` is an alias); a bare
  `Throw` inside Catch rethrows. `Err` has Number, Description and Line.
- Finally runs on every way out: normal end, Exit, Return, an error.
  An error inside Finally replaces the pending one.
- No GoTo into or out of a Try block.
- An error nobody catches restores the terminal, prints
  `file:line: runtime error N: message` and exits with status 1.

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
