# TermForms examples

Each builds with `tfc` and runs in a terminal (macOS, Linux, Windows):

| Example | Shows | Build |
| --- | --- | --- |
| `hello.bas` | one form, a TextBox, a button and its Click | `tfc hello.bas` |
| `demo/` | every v1 control, menus, dialogs, several forms, Timers, DoEvents, the mouse, an event log | `tfc demo/demo.vbp` |
| `addressbook/` | a small real utility: a Type in a Dictionary, a text file read and written, Find as you type, menus and Shortcuts, MsgBox questions | `tfc addressbook/addressbook.vbp` |
| `viewer/` | a folder's files (Dir$) and a file's lines, size and date; menus with a checked item | `tfc viewer/viewer.bas` |
| `samples/` | one short program each for Select Case, Types and Refs, errors, arrays, strings and Dictionary, showing its results | `tfc samples/types.bas` |

`demo/demo.vbp` is also the program for checking a terminal: run it and
try the keys and the mouse. The address book keeps `addresses.txt` in the
folder it runs in.

Each example is also a run test (`tests/run/demo.vbp`,
`addressbook.vbp`, `viewer.vbp`, `sample_*.vbp`), built from these files,
so none can stop working unnoticed.
