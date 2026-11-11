# TermForms BASIC: the programmer's guide

TermForms BASIC is a language with VB-DOS-style syntax for event-driven
forms programs that run in a text terminal: on your own machine, or on a
server you reach over SSH. You design a form of controls, write `Sub`s
that answer their events, and `tfc` turns the program into one native
executable with no run-time dependencies.

This guide is for programmers, and assumes you have met BASIC, Visual
Basic or VB.NET before. It shows how to write and build programs. For
every rule in full, see [REFERENCE.md](REFERENCE.md); for a one-page
summary of what works today, [LANGUAGE.md](LANGUAGE.md).

**Contents**

1. [Getting started](#1-getting-started)
2. [Coming from Visual Basic](#2-coming-from-visual-basic)
3. [The language](#3-the-language)
4. [Forms and controls](#4-forms-and-controls)
5. [Files, folders and the system](#5-files-folders-and-the-system),
   and console programs
6. [Calling C](#6-calling-c)
7. [Building, debugging and deploying](#7-building-debugging-and-deploying)

---

## 1. Getting started

### Installing

Unpack a TermForms release, add its `bin` folder to your PATH, and install
a C compiler, which `tfc` drives: the Command Line Tools on macOS
(`xcode-select --install`), gcc or clang on Linux, and on Windows zig
(`winget install zig.zig`). The `README.md` in the release has the details
for each platform. Check with:

```sh
tfc --version
```

### A first program

A form program is a `.bas` file that starts with a **form block**,
describing the form and its controls, followed by the code:

```vb
' greet.bas -- asks for a name and greets it.
Begin Form frmGreet
    Caption = "Greetings"
    Width = 40
    Height = 9
    Begin Label lblName
        Caption = "&Name:"
        Left = 2
        Top = 1
        Width = 6
    End
    Begin TextBox txtName
        Left = 9
        Top = 1
        Width = 25
    End
    Begin CommandButton cmdGreet
        Caption = "&Greet"
        Left = 9
        Top = 3
        Width = 12
        Default = True
    End
    Begin Label lblGreeting
        Left = 2
        Top = 5
        Width = 34
    End
End

Sub cmdGreet_Click()
    Dim who As String = Trim$(txtName.Text)
    If who = "" Then
        who = "world"
    End If
    lblGreeting.Caption = "Hello, " & who & "!"
End Sub
```

Build it and run it:

```sh
tfc greet.bas
./greet                 (Windows: greet.exe)
```

The form appears centred in the terminal. Type a name, press Enter (the
Default button, shown as `« Greet »`) or click Greet, and the label
changes. Tab moves between
the controls, Alt+N jumps to the box (the `&` in a Caption makes an access
key), and **Ctrl+F4 closes the form**, which ends the program.

`Sub cmdGreet_Click` runs because of its name: `<control>_<event>`. There
is no wiring to do, and the compiler warns when a Sub's name matches a
control's event in all but letter case, as it would never be called.

### A program for a Linux server

A server needs nothing installed to run a TermForms program. Build a
static Linux executable on your own machine with zig, copy it across and
run it over SSH:

```sh
tfc --cc "zig cc -target x86_64-linux-musl" -o greet-linux greet.bas
scp greet-linux server:
ssh -t server ./greet-linux
```

Copied from Windows, the file arrives without its executable bit: run
`chmod +x greet-linux` on the server once. Dropping the connection while
it runs leaves the server's terminal as it was. [Building for other
machines](#building-for-other-machines) has the other targets, Windows
among them, and how to get zig.

### Several files: modules and projects

A program can be several `.bas` files: one per form, and code modules
with none. Give `tfc` them all, or write a **project file**:

```text
' inventory.vbp
Name=Inventory
Startup=frmMain
Form=main.bas
Form=edit.bas
Module=stock.bas
```

`tfc inventory.vbp` builds `Inventory`. `Startup=` names the form to show
first, or `Sub Main` to start in code; without it the program starts at
Sub Main if there is one, else the first form. See
[Modules](#modules-and-visibility) for how the files see each other.

A project can also set the colours its forms are drawn in, one line per
colour changed, each written `#RRGGBB`:

```text
FormBackColor=#003366
HighlightBackColor=#FFCC00
```

Forms are drawn in exact colours, so they look the same in every
terminal whatever its colour scheme. The defaults are the DOS look: blue
forms with white text and yellow access keys, grey menus and dialogs
with black text and red access keys, the focus black on cyan, on a dark
grey backdrop. REFERENCE.md "Colours" lists every colour you can set.

---

## 2. Coming from Visual Basic

TermForms keeps VB's syntax and spirit, but a handful of rules differ, and
these are the ones that catch people out:

| Rule | TermForms | VB6 | VB.NET |
| --- | --- | --- | --- |
| Names | **case-sensitive**: `count` and `Count` differ (keywords are not) | case-insensitive | case-insensitive |
| Arrays | **0-based**: `Dim a(10)` is 11 elements, `a(0)` to `a(10)`; no `Option Base` | same, but Option Base 1 exists | same |
| Parameters | **ByVal** unless marked ByRef | ByRef | ByVal |
| An array passed ByVal | read-only in the callee | a copy | elements changeable |
| Integer, Long | 32-bit, 64-bit | 16, 32 | 32, 64 |
| Variants | none: every variable has a type | the default | Object |
| Option Explicit / Strict | always on / on: no implicit narrowing, no String↔number | optional | optional |
| Errors | `Try` / `Catch` / `Finally`, `Throw` | `On Error GoTo` | `Try`/`Catch` |
| Objects | no classes: `Type` records, and `Ref` to share one | classes | classes |
| `Dim` in a block | visible in that block, **fresh on every pass of a loop** | whole procedure | block, keeps its value |
| `And`, `Or` | both sides always evaluated | same | same (AndAlso, OrElse short-circuit) |
| String positions | from 1 (`Mid$(s, 1, 3)`) | same | 0 in .NET methods |

Also missing on purpose: `GoTo`, `GoSub`, `On Error`, `Set`, `Variant`,
`Option Base`, default properties, user classes. And a few things are
TermForms' own: `Ref` types, `Dictionary(Of T)` built in, `Exists(path)`,
`Platform`, and Declares that call C.

---

## 3. The language

### Names, types and variables

```vb
Dim count As Integer            ' 32-bit, -2147483648 to 2147483647
Dim total As Long               ' 64-bit
Dim price As Double
Dim name As String              ' UTF-8 text
Dim done As Boolean
Dim b As Byte                   ' 0 to 255, for binary data
Dim n%, big&, x#, s$            ' the type suffixes still work
Dim a, b2 As String             ' both Strings (VB.NET's rule)
Dim start As Integer = 1, title As String = "Orders"   ' with values
```

Every variable needs a type; there are no Variants, and a name used
without a Dim is an error. A variable starts as 0, `""` or False. A `Dim`
inside an `If`, loop or other block belongs to that block, and **inside a
loop it starts afresh on each pass**.

Names are case-sensitive: `Total` and `total` are two names. Keywords are
not (`dim` is `Dim`), and neither are module names, which come from file
names. Reserved words cannot be names.

**Conversions** follow VB.NET's Option Strict. Widening is automatic
(Byte → Integer → Long → Double); narrowing needs `CInt`, `CLng` or
`CByte`; numbers and text never convert by themselves, except through `&`:

```vb
total = count                   ' Integer to Long: fine
count = CInt(total)             ' Long to Integer: needs CInt (Overflow if it does not fit)
s = "Total: " & total           ' & turns numbers into text
n = CInt(txtQty.Text)           ' text to a number: CInt, CLng, CDbl (error 13 if not a number)
n = Val("12 apples")            ' Val is lenient: 12
```

Arithmetic is checked: an Integer or Long that overflows is error 6, and
division by zero is error 11 (never Infinity). `/` always gives a Double,
`\` divides whole numbers, `Mod` keeps the left operand's sign. `CInt`
rounds halves to even, as VB (`CInt(2.5)` is 2); `Fix` truncates, `Int`
rounds down. A Double prints as the shortest text that reads back exactly
(`0.1`, `0.30000000000000004`).

### Constants and Enums

```vb
Const MaxItems = 100                ' an Integer, from its value
Const Rate = 0.075                  ' a Double
Const Title As String = "Orders"
Const Limit = MaxItems * 2          ' worked out by the compiler

Enum Colour
    Red                             ' 0
    Green                           ' 1
    Blue = 10
End Enum

Dim c As Colour = Colour.Blue       ' always written qualified
Dim n As Integer = c + 1            ' an Enum widens to Integer
c = CType(n, Colour)                ' and back needs CType
```

VB's named constants are built in: `vbYesNo`, `vbQuestion`, `vbCrLf`,
`vbTab`, `vbKeyReturn` and the rest.

### Decisions and loops

```vb
If total > 100 Then
    discount = 10
ElseIf total > 50 Then
    discount = 5
Else
    discount = 0
End If
If done Then Exit Sub                   ' one line

Select Case score
    Case 100
        grade = "perfect"
    Case 90 To 99, 85
        grade = "A"
    Case Is >= 50
        grade = "pass"
    Case Else
        grade = "fail"
End Select

For i = 1 To 10 Step 2 ... Next i
For i As Integer = 0 To UBound(names) ... Next  ' i belongs to the loop
For Each w As String In words ... Next
Do While Not EOF(1) ... Loop                    ' or Do Until, or the test at the bottom
While queue > 0 ... Wend                        ' End While works too
```

`Exit For`, `Exit Do`, `Exit While`, `Exit Sub` and `Exit Function` leave
the innermost one of their kind. A Case may hold any expression; the
first matching Case runs, Strings compare exactly (case matters), and the
compiler warns of a Case that an earlier one always beats.

### Subs and Functions

```vb
Function Area(w As Integer, h As Integer) As Integer
    Return w * h                        ' or: Area = w * h
End Function

Sub Swap(ByRef a As Integer, ByRef b As Integer)
    Dim t As Integer = a
    a = b
    b = t
End Sub

Swap x, y                               ' or Swap(x, y), or Call Swap(x, y)
n = Area(3, 4)
```

Parameters are **ByVal unless marked ByRef**: a Sub can change its caller's
variable only through a ByRef parameter, and the argument must then be a
variable, element or field. A Type passed ByVal is copied; an array passed
ByVal is read-only (pass it ByRef to change it). Parentheses at the call
never change ByRef into ByVal, as they did in VB6. Expressions are worked
out strictly left to right.

### Arrays

```vb
Dim scores(9) As Integer                ' fixed: 10 elements, 0 to 9
Dim grid(24, 79) As String              ' two dimensions
Dim names() As String                   ' dynamic: empty until sized
Dim days() As String = {"Mon", "Tue", "Wed"}

ReDim names(n)                          ' size it
ReDim Preserve names(n + 10)            ' grow it, keeping what is there
For i As Integer = 0 To UBound(names)   ' LBound is always 0; UBound -1 when empty
copy = names                            ' copies the elements
Erase names                             ' empty again
```

Every index is checked (error 9 outside the array). Arrays can hold
Types, Refs and Strings as well as numbers. `For Each` visits every
element; a two-dimensional array in storage order, the first index
changing fastest.

### Types, Refs and With

A **Type** is a record, and a value: assigning one copies it.

```vb
Type Address
    Street As String
    Town As String
End Type

Type Customer
    Name As String
    Home As Address                     ' a Type inside a Type
    Orders() As Long                    ' an array inside a Type
End Type

Dim c As Customer
c.Name = "Ann"
c.Home.Town = "Perth"
Dim twin As Customer = c                ' a separate copy
```

A **Ref** shares one record between variables: `New` makes it, `Nothing`
is no record, and `Is` / `IsNot` compare. Refs let a Type point at
others of its kind, so they build lists and trees:

```vb
Type Node
    Value As String
    NextNode As Ref Node
End Type

Dim head As Ref Node                    ' Nothing
Dim n As New Node                       ' a Ref and a new record
n.Value = "first"
n.NextNode = head
head = n                                ' head and n share the record
If head IsNot Nothing Then MsgBox head.Value
```

Using a Ref that is Nothing is error 91. Records are freed when nothing
refers to them any more (but a cycle of Refs is never freed).

`With` names a target once:

```vb
With orders(i)
    .Customer = "Ann"
    .Lines(0).Qty = 2
End With
```

### Dictionaries

```vb
Dim ages As Dictionary(Of Integer)      ' String keys, Integer values
ages("Ann") = 30                        ' adds or replaces
ages.Add "Bo", 25                       ' error 457 if Bo is there
If ages.Exists("Cy") Then n = ages("Cy")
ages.Remove "Bo"
For Each name As String In ages.Keys    ' in the order they were added
    total = total + ages(name)
Next
```

Keys are Strings, case-sensitive; values may be any type but an array or
a Dictionary, Types included (`contacts(name).Phone`). Reading a key that
is not there is error 5: ask `Exists` first. A Dictionary is a value, like
an array: ready at its Dim, copied by assignment.

### Strings

Strings are UTF-8 and immutable. Positions count characters (code
points) **from 1**, as in VB:

```vb
Dim s As String = "Hello, wörld"
Len(s)                  ' 12 characters (LenB(s) is 13 bytes)
Mid$(s, 8, 3)           ' "wör"
Left$(s, 5), Right$(s, 5)
InStr(s, "w")           ' 8; 0 if not found
Replace(s, "Hello", "Goodbye")
UCase$(s), LCase$(s)    ' A-Z and a-z only, for now
Trim$(s), LTrim$(s), RTrim$(s)
Split("a,b,c", ",")     ' an array; Join(parts, ", ") the reverse
Mid$(s, 1, 5) = "Howdy" ' the Mid$ statement replaces in place
```

Comparison is exact (case-sensitive). `Str$(5)` keeps VB's leading space
(`" 5"`); `CStr(5)` and `"" & 5` give `"5"`.

### Errors

```vb
Try
    total = total \ count
Catch When Err.Number = 11
    total = 0                           ' division by zero
Catch
    MsgBox Err.Description & " at line " & Err.Line
    Throw                               ' pass it on
Finally
    Close #1                            ' runs however the Try is left
End Try

Throw 1000, "An order needs a customer"  ' your own error (1000 and up)
```

Runtime errors keep VB6's numbers and texts (53 File not found, 9
Subscript out of range, ...), so `Catch When Err.Number = 53` means what
it always has. An error nobody catches restores the terminal, prints
`file:line: runtime error N: message`, and ends the program. `End` stops
at once, even inside a Try (no Finally runs).

### Modules and visibility

Each `.bas` file is a module named after the file (`stock.bas` is
`stock`). Subs, Functions, Types and Enums are **Public** unless marked
`Private`; module-level variables and Consts are **Private** unless
declared `Public`:

```vb
' stock.bas
Public total As Long                    ' seen by every module
Dim cache As String                     ' this module only
Sub AddStock(qty As Integer)            ' Public
    total = total + qty
End Sub
```

Another module writes `AddStock 5`, or `stock.AddStock 5` when two modules
have the same name. A form is known everywhere, and so are its controls:
`frmMain.txtName.Text`.

---

## 4. Forms and controls

### The form block

A form's `.bas` file starts with its form block: `Begin Form name`, its
properties, its controls (each a `Begin <type> name ... End`), and `End`.
Sizes are in character cells; `Width` and `Height` include the border,
and a control's `Left` and `Top` count from inside it (with a menu bar,
from the row below it). The form is centred in the terminal; if the
terminal is too small, it says so until it is resized.

Values in the form block are literals: numbers, `"text"`, `True`/`False`.
Tab order is the order the controls are written in.

### The controls

| Control | Properties (besides Left, Top, Width, Height, Enabled, Visible) | Events |
| --- | --- | --- |
| Label | Caption (words wrap if Height > 1) | Click |
| TextBox | Text, SelStart, SelLength, SelText, MaxLength, Locked, PasswordChar; MultiLine, ScrollBars (form block only) | Change, GotFocus, LostFocus, KeyPress, KeyDown |
| CommandButton | Caption, Default, Cancel | Click (and the focus and key events) |
| CheckBox | Caption, Value (0, 1, 2 = grey) | Click |
| OptionButton | Caption, Value (Boolean) | Click |
| Frame | Caption; holds controls, positioned inside it | (none) |
| ListBox | Text, ListIndex, ListCount, MultiSelect, Sorted, SelCount, TopIndex; `List(i)`, `Selected(i)`; AddItem, RemoveItem, Clear | Click, DblClick |
| ComboBox | Text, ListIndex, ListCount, Sorted, Style (form block only), SelStart, SelLength, SelText, MaxLength, Locked; `List(i)`; AddItem, RemoveItem, Clear | Click, Change, DropDown |
| Timer | Interval (ms), Enabled | Timer |
| Menu | Caption, Checked, Shortcut, Enabled, Visible | Click |

Every control that can take the focus also has CausesValidation, the
SetFocus method, and the events GotFocus, LostFocus, Validate, KeyPress
and KeyDown.

The form itself has Caption, Width, Height, Visible (read-only) and the
events Load, Activate and Unload. `Me` is the form whose module the code
is in.

An access key (the letter after `&` in a Caption) is shown in its own
colour (yellow on a form, red on a menu or dialog) and underlined; `&&`
shows an `&`. A Label showing data, such as a file name, should double
any `&` in it (`Replace(text, "&", "&&")`).

**Enabled and Visible** work on every control and menu, in the form block
or in code. A disabled control is drawn dim, Tab passes over it, and it
takes no keys, access keys or clicks; a hidden one is not drawn at all.
Setting either on a Frame does the same to everything inside it, so a
whole section of a form can be switched off at once:

```vb
Sub chkShip_Click()
    fraAddress.Enabled = (chkShip.Value = vbChecked)   ' the Frame and its controls
    cmdPrint.Visible = Not cmdPrint.Visible
End Sub
```

A **Frame** holds the controls written inside its block; OptionButtons in
one Frame are a group of their own, so a form can have several groups.
A **TextBox** edits as a Windows edit box does: Shift+arrows select,
Ctrl+arrows move by words, Ctrl+Z undoes, Ctrl+X/C/V cut, copy and paste
(through the program's own clipboard). Setting Text or SelText in code
fires Change, as in VB.

A TextBox with `MultiLine = True` (set in the form block, with a Height
of more than one row) holds several lines. **Enter starts a new line** in
it, never pressing the Default button; Tab still moves on. Its lines are
separated by `vbLf` in Text: text you set with `vbCrLf` (or a paste) has
its line breaks made `vbLf`, so split it with `Split(txt.Text, vbLf)`.
`ScrollBars` is 0 (none), 1 (horizontal), 2 (vertical) or 3 (both); without
a horizontal bar long lines wrap at the box's width, as in VB.

```vb
Begin TextBox txtNotes
    Width = 40
    Height = 8
    MultiLine = True
    ScrollBars = 2      ' a vertical bar; lines wrap
End
```

A **ComboBox** is a ListBox's items with one shown and a list that drops
down (F4 or Alt+Down, or a click on its arrow; Up and Down choose without
opening it). With `Style = 2` the user can only choose from the list;
with `Style = 0` (the default) they can also type, and `ListIndex` is -1
when what they typed is not an item. DropDown fires just before the list
opens, so a program can fill it then.

```vb
Begin ComboBox cboSize
    Width = 14
    Style = 2           ' choose from the list only
End
```

A **ListBox** with `MultiSelect = 1` toggles items
with Space; with 2, arrows choose, Shift+arrows choose a run of items,
and Ctrl+arrows move without choosing, so Space can add others. A **Timer** raises its
event every Interval milliseconds while the form waits for input.

### Events

```vb
Sub Form_Load()                         ' before the form first shows
Sub Form_Activate()                     ' each time it comes to the top
Sub Form_Unload(ByRef Cancel As Integer) ' Cancel = 1 refuses to close
Sub txtQty_KeyPress(ByRef KeyAscii As Integer) ' change it, or 0 to drop the key
Sub txtQty_KeyDown(ByRef KeyCode As Integer, Shift As Integer)
Sub txtQty_GotFocus() / _LostFocus() / _Change()
Sub txtQty_Validate(ByRef Cancel As Boolean)   ' before the focus leaves
Sub lstItems_Click() / _DblClick()
Sub tmrClock_Timer()
```

Handlers that may change their argument take it **ByRef**, as above (the
compiler shows the right form if one is wrong). For example, a box that
takes only digits:

```vb
Sub txtQty_KeyPress(ByRef KeyAscii As Integer)
    If KeyAscii >= 32 And (KeyAscii < Asc("0") Or KeyAscii > Asc("9")) Then
        KeyAscii = 0
    End If
End Sub
```

**Checking an entry when the user leaves it** is Validate's job, as in
VB6. It runs before the focus moves to another control (by Tab, a click,
an access key or `SetFocus`) and before Enter or Esc presses the Default
or Cancel button; `Cancel = True` keeps the user where they are, and the
move (or the button) does not happen. A control whose `CausesValidation`
is False is left out: give it to a Cancel or Help button so the user can
always get away from a half-filled form. Closing the form never runs
Validate. `ctl.SetFocus` moves the focus in code (error 5 if the control
is hidden or disabled).

```vb
Sub cboCity_Validate(ByRef Cancel As Boolean)
    If cboCity.ListIndex = -1 And cboCity.Text <> "" Then
        cboCity.AddItem cboCity.Text           ' typed, not chosen: keep it
    ElseIf cboCity.Text = "" Then
        MsgBox "Where?", vbExclamation
        Cancel = True                          ' the focus stays here
    End If
End Sub
```

### Keys

| Key | Does |
| --- | --- |
| Tab, Shift+Tab | the next, previous control |
| Arrows | move within an OptionButton group or a list |
| Space | press a button, toggle a CheckBox, choose an OptionButton |
| Enter, Esc | press the Default, the Cancel button |
| Alt+letter | the control (or menu) whose Caption has `&letter` |
| F10 or Ctrl+B | the menu bar |
| Ctrl+F4 | close the form |

On a Mac, Alt+letter needs the terminal to send Option as Alt (iTerm2:
Profiles > Keys > Left Option key: Esc+; Terminal.app: Profiles >
Keyboard > Use Option as Meta key; Ghostty: `macos-option-as-alt = left`).
A program notices when it does not, and says so once.

### The mouse

The mouse works in every terminal that reports it (most do, over SSH
too): a click focuses and presses, a drag selects text in a TextBox, a
double-click chooses a word or a list item, and the wheel scrolls a list
or a multi-line TextBox (a press on its scroll bar jumps there).
Menus open and choose by clicking. Shift+drag still selects text on the
screen for copying. Set `TERMFORMS_MOUSE=0` to leave the mouse to the
terminal.

### Colours in older terminals

Forms are drawn in exact colours, which every current terminal shows. In
one that cannot (tmux without its RGB setting, some older terminals),
the colours come out wrong or not at all; there, the user sets
`TERMFORMS_COLOR=256` (the nearest of 256 colours) or `TERMFORMS_COLOR=16`
(the terminal's sixteen, as its scheme shows them).

### Menus

Menus are written in the form block, two levels deep:

```vb
Begin Form frmEdit
    Caption = "Editor"
    Width = 50
    Height = 12
    Begin Menu mnuFile
        Caption = "&File"
        Begin Menu mnuSave
            Caption = "&Save"
            Shortcut = "Ctrl+S"
        End
        Begin Menu mnuSep
            Caption = "-"
        End
        Begin Menu mnuExit
            Caption = "E&xit"
        End
    End
    Begin Menu mnuView
        Caption = "&View"
        Begin Menu mnuWrap
            Caption = "&Wrap lines"
            Checked = True
        End
    End
End

Sub mnuSave_Click()
    MsgBox "Saved.", vbInformation
End Sub

Sub mnuExit_Click()
    Unload Me
End Sub

Sub mnuView_Click()                     ' runs as the View menu opens
    mnuWrap.Enabled = True
End Sub

Sub mnuWrap_Click()
    mnuWrap.Checked = Not mnuWrap.Checked
End Sub
```

A Caption of `-` is a separator. A Shortcut is `Ctrl+letter`, `F1` to
`F12` (with Shift+, Ctrl+ or Alt+), or `Del`; it works whenever the form
is on top. Opening a menu fires its own Click first, so the program can
set its items' Checked and Enabled. Choosing an item never changes Checked
by itself.

### Dialogs

```vb
MsgBox "Saved."                                         ' a statement
If MsgBox("Delete it?", vbYesNo + vbQuestion, "Delete") = vbYes Then ...
name = InputBox("Your name:", "Hello", "Ann")           ' "" if cancelled
```

The buttons and icons are VB's (`vbOKCancel`, `vbYesNoCancel`,
`vbExclamation`, `vbDefaultButton2`, ...), and so are the answers
(`vbYes`, `vbNo`, `vbCancel`). A dialog's lines break at `vbLf` or
`vbCrLf`.

### Several forms

Each form is in its own module. `frmOptions.Show vbModal` shows a form and
waits until it is hidden or unloaded; `frmOptions.Show` shows it and goes
on. Only the top form takes keys. `Hide` takes a form off the screen,
keeping its controls' values; `Unload` closes it (asking its Form_Unload),
and **the next Show makes it afresh**, its controls back to their
form-block values, as in VB:

```vb
Sub cmdOptions_Click()
    frmOptions.Show vbModal
    lblLevel.Caption = "Level: " & frmOptions.txtLevel.Text
End Sub
```

The program ends when its last form closes.

### Long jobs: DoEvents

Code runs while the form waits; a long loop would freeze it. `DoEvents`
lets the form handle what is waiting (keys, clicks, Timers) and redraw,
then returns:

```vb
For i As Integer = 1 To 100000
    Work i
    If i Mod 100 = 0 Then
        lblProgress.Caption = i & " done"
        DoEvents                        ' keeps the form live
        If stopping Or Not Me.Visible Then Exit For
    End If
Next
```

A handler may run again inside DoEvents (a button pressed twice), as in
VB; guard with a flag if that would be wrong. If the user closes the form
meanwhile, its Visible is False when DoEvents returns.

### Text on screen

Text is UTF-8 throughout. Wide characters (Chinese, Japanese, Korean,
emoji) take two cells and line up correctly; combining accents are not
drawn (text with ready-made accented letters, the usual kind, shows in
full).

---

## 5. Files, folders and the system

### Text files

```vb
Dim n As Integer = FreeFile
Open "notes.txt" For Output As #n       ' Output, Append or Input
Print #n, "Total: "; total              ' items joined as & joins them
Print #n, "no newline yet";             ' a trailing ; leaves out the newline
Close #n

Open "notes.txt" For Input As #1
Do While Not EOF(1)
    Dim line As String
    Line Input #1, line
Loop
Close #1
```

Text files are UTF-8, written with LF line ends on every platform and
read with LF or CRLF. Errors keep VB's numbers (53 File not found, 76 Path
not found, 54 Bad file mode, ...). Relative names start from the folder
the program runs in.

### Binary files

```vb
Dim buf() As Byte
Open "data.bin" For Binary As #1
ReDim buf(511)
Get #1, 0, buf                          ' 512 bytes from offset 0 (offsets count from 0)
Get #1, , buf                           ' the next 512
Put #1, 1024, buf                       ' write at offset 1024
Close #1
```

### Folders and files

| | |
| --- | --- |
| `Dir$("*.txt")`, then `Dir$()` | each matching file in turn (sorted), then `""` |
| `Files("photos/*")`, `Folders("photos/*")` | the names of the files, or of the folders, as a sorted String array (assign it, or For Each) |
| `IsLink(path)` | True if path is a symbolic link (Windows: or a junction) |
| `Exists(path)` | True if a file or folder is there |
| `FileLen(path)`, `FileDateTime(path)` | the size; when it changed ("yyyy-mm-dd hh:mm:ss") |
| `Kill`, `FileCopy`, `Name old As new` | delete, copy, rename |
| `MkDir`, `RmDir`, `ChDir`, `CurDir$` | folders |

`/` separates folders everywhere (and `\` too on Windows).

To go through a whole tree, use `Files` and `Folders` rather than `Dir$`:
each call gives its own list, so a Sub can call itself for each folder.
A link to a folder is listed by `Files`, not `Folders`, so the walk
cannot go round in a loop. Note the `/*`: `Folders("photos")` gives
`photos` itself, not what is inside it.

```vb
Sub Walk(path As String)
    For Each name As String In Files(path & "/*")
        Print path & "/" & name & "  " & FileLen(path & "/" & name)
    Next
    For Each name As String In Folders(path & "/*")
        Walk path & "/" & name
    Next
End Sub
```

### The system

| | |
| --- | --- |
| `CommandArgs()` | the program's arguments, one by one, as a String array (assign it, or For Each) |
| `Command$` | the program's arguments, joined by spaces |
| `App.Path`, `App.EXEName`, `App.FullPath` | the executable's folder, its name (without .exe), its full path |
| `Environ$("HOME")` | an environment variable ("" if not set) |
| `Shell("ls -l")` | runs a command through the shell, waits, gives its exit code |
| `Now`, `Date$`, `Time$`, `Timer` | the date and time; seconds since midnight |
| `Platform` | "Windows", "macOS" or "Linux": what the program was built for |

### Console programs

A program with a `Sub Main` and no forms can work as an ordinary
command-line tool: reading standard input, writing standard output and
error, and giving the shell an exit code. This one numbers the lines it
is given:

```vb
' number.bas -- writes its input with line numbers:  number < notes.txt
Sub Main()
    Dim count As Integer
    Do While Not EOF(0)                 ' EOF(0): the end of standard input
        Dim line As String
        Line Input line
        count = count + 1
        Print Right$("    " & count, 5); "  "; line
    Loop
    If count = 0 Then
        Print #0, "number: no input"    ' #0 is standard error
        End 1                           ' the exit code
    End If
End Sub
```

- `Print` follows `Print #`'s rules: `;` joins items, a trailing `;`
  leaves off the newline, and lines end with LF.
- `Line Input "Name: "; name` writes the prompt, then reads a line (typed,
  or from a pipe or file) without its line end. Reading past the end is
  error 62.
- `End n` ends the program with exit code n (0 to 255); finishing Sub Main
  gives 0, and an uncaught error 1.
- Text goes through as UTF-8, and to a Windows console as Unicode, so
  accented letters and the like show properly there too.
- A form program can Print as well: while a form is on screen the text is
  kept, and written when the program ends (or a Shell gives the terminal
  back). Line Input then is error 900, as the keys belong to the form.

---

## 6. Calling C

A `Declare` makes a C function callable like any Function or Sub. `Lib`
says where it is: a C file of your program, a library, or (with `""`) the
C library itself:

```vb
Declare Function add_tax Lib "lib/utils.c" (ByVal cents As Integer) As Integer
Declare Function sqlite3_libversion Lib "sqlite3" () As String
Declare Function strlen Lib "" (ByVal s As String) As CSizeT
Declare Sub fill_name Lib "lib/utils.c" (ByRef buffer As String)
```

```c
/* lib/utils.c */
#include <stdint.h>
int32_t add_tax(int32_t cents) { return cents + cents / 10; }
```

- A C file is found relative to the `.bas` file, compiled with the
  program (its warnings and errors shown), and cross-compiles with it. A
  library is linked in when the program is built.
- `tfc` writes each C function's prototype from its Declare, so the C
  must use exactly these types: Integer is `int32_t`, Long `int64_t`,
  Double `double`, Byte `uint8_t`, Boolean `int` (0 or 1); ByRef adds a
  `*`.
- **C's `long` is 64-bit on Linux and macOS but 32-bit on Windows.** For a
  C function taking `long`, `unsigned long` or `size_t`, declare the
  parameter `CLong`, `CULong` or `CSizeT`: a Long in BASIC, the right size
  in C everywhere (and error 6 if a value does not fit).
- A ByVal String reaches C as `const char *` (UTF-8); a String result is
  copied. A ByRef String is a buffer C may fill: give it room first
  (`buffer = Space$(100)`), and afterwards it holds what C wrote, up to
  the NUL.
- An element passed ByRef (`bytes(0)`) gives C a pointer into the array;
  a Type of numbers passed ByRef gives a pointer to a matching struct.
- `Alias "cname"` calls a C function by another name. There are no
  callbacks yet.

---

## 7. Building, debugging and deploying

### tfc

```text
tfc [options] file.bas [more.bas ...]      or      tfc [options] project.vbp

  -o <file>       the program's name (default: after the first file or Name=)
  --release       optimise, without debugging information
  --cc <command>  the C compiler, e.g. "zig cc -target x86_64-linux-musl"
                  (default: $TFC_CC, else cc; on Windows zig cc, else cl)
  --emit-c        write the generated C and stop (-o - to standard output)
  --keep-c        keep the generated C next to the program
  --runtime <dir> the runtime to build with (default: $TERMFORMS_RUNTIME,
                  else runtime/ beside tfc's bin folder)
  -v              show the C compiler's command and messages
```

Your mistakes are reported as `file:line:col: error: ...`, in BASIC's
terms; you never see the C compiler's warnings unless they are about C of
your own (Declares) or you ask with `-v`. If the C compiler fails, tfc
shows its first messages; when you named it with `--cc` or `TFC_CC`, check
that command first (a missing `-target` is the usual slip).

### Testing without a terminal

Set `TERMFORMS_HEADLESS` to a key script and the program runs without a
terminal: it takes the keys from the script and prints the final screen.
That makes forms programs testable in scripts and CI:

```sh
TERMFORMS_HEADLESS='Ann{TAB}{ENTER}' TERMFORMS_HEADLESS_SIZE=60x14 ./greet
```

Special keys are written in braces: `{ENTER}`, `{TAB}`, `{ESC}`, `{F1}`,
`{CTRL+S}`, `{ALT+N}`, `{SHIFT+LEFT}`, `{CLICK x,y}`, `{WAIT 500}` (time
passing, for Timers) and `{IDLE}` (where a DoEvents returns).

### Debugging

Programs are built with debugging information unless `--release`, and
each BASIC line is one line to the debugger: set breakpoints on `.bas`
lines, step from line to line and into your own Subs. TermForms' printers
show Strings as text and arrays and Dictionaries by element:

```text
lldb ./greet                        (macOS, Linux)
(lldb) command script import /path/to/termforms/debug/lldb/termforms.py
(lldb) breakpoint set -f greet.bas -l 31
(lldb) run

gdb ./greet                         (Linux)
(gdb) source /path/to/termforms/debug/gdb/termforms.py
```

A variable keeps its C name there: `x` is `v_x` in a Sub, `g_x` at
module level, and a Type's members are `f_<name>`.

In **Visual Studio Code**, the extension in `editors/vscode` gives
highlighting, `tfc`'s errors in the Problems list and breakpoints in
`.bas` files; with CodeLLDB, F5 builds and debugs. Its README says how to
set it up.

### Building for other machines

`tfc` builds TermForms' runtime from source with every program, so it can
build for any machine its C compiler can: a Mac or a Windows PC can build
for a Linux server, and a Mac can build for Windows. zig is the C
compiler that makes this easy, as it carries the C library of every
system it targets. You need zig 0.16 or newer on the machine you build
on:

- **Windows:** `winget install zig.zig` (the zig you may already use).
- **macOS:** `brew install zig` (the Command Line Tools build only for
  the Mac itself).
- **Linux:** your distribution's `zig` package, or a download from
  ziglang.org.

Then name the target with `--cc`:

| To run on | `--cc` | Name the program |
| --- | --- | --- |
| Linux x86-64, any distribution (one static file) | `"zig cc -target x86_64-linux-musl"` | `-o greet-linux` |
| Linux on ARM64 (a Raspberry Pi 4 or 5 running 64-bit, an ARM server) | `"zig cc -target aarch64-linux-musl"` | `-o greet-arm` |
| Windows x64 (10 version 1809 or later) | `"zig cc -target x86_64-windows-gnu"` | `-o greet.exe` |

For example, on a Mac:

```sh
tfc --cc "zig cc -target x86_64-linux-musl" --release -o greet-linux greet.bas
tfc --cc "zig cc -target x86_64-windows-gnu" --release -o greet.exe greet.bas
```

- Give each target its own `-o` name, so one build does not overwrite
  another (or the program for your own machine).
- A project builds the same way: `tfc --cc "..." -o book-linux book.vbp`.
- Setting `TFC_CC` saves typing `--cc` each time:
  `export TFC_CC="zig cc -target x86_64-linux-musl"` (Windows PowerShell:
  `$env:TFC_CC = "zig cc -target x86_64-linux-musl"`). Unset it to build
  for your own machine again.
- `--release` leaves out the debugging information, for a smaller file to
  ship. A Windows build also writes a `.pdb` file beside the `.exe`; only
  a debugger needs it, so ship the `.exe` alone.
- The first build for a target is slow (a minute or more): zig builds that
  target's C library once and keeps it, and later builds are quick.
- A program with `Declare`s: C files of its own (`Lib "utils.c"`) are
  compiled with the same `--cc`, so they cross-build too; a library
  (`Lib "sqlite3"`) must be one zig can link for that target, which a
  library installed only on your own machine is not.
- A Mac program is built on a Mac (with the Command Line Tools).

Check a cross-built program where it will run, in the terminal its users
will have: copy it across and run it, or run it headless there
([Testing without a terminal](#testing-without-a-terminal)) to see it
start without a terminal at all.

### Deploying

A built program is one file. For a Linux server, cross-build a static
executable (see [Building for other machines](#building-for-other-machines))
and copy it with `scp`; nothing else is needed there. Copied from Windows,
it arrives without its executable bit: `chmod +x` it there once. On Windows, a program runs in Windows
Terminal or the console (Windows 10 1809 or later); double-clicking one
opens a window for it.

Programs may be distributed freely: the runtime built into them is under
the 0BSD licence, which asks nothing of you or your users.
