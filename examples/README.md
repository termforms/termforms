# TermForms examples

Each builds with `tfc` and runs in a terminal (macOS, Linux, Windows):

| Example | Shows | Build |
| --- | --- | --- |
| `hello.bas` | one form, a TextBox, a button and its Click | `tfc hello.bas` |
| `demo/` | every v1 control, menus, dialogs, several forms, Timers, DoEvents, the mouse, an event log | `tfc demo/demo.vbp` |
| `addressbook/` | a small real utility: a Type in a Dictionary, a text file read and written, Find as you type, menus and Shortcuts, MsgBox questions | `tfc addressbook/addressbook.vbp` |
| `viewer/` | a folder's files (Dir$) and a file's lines, size and date; menus with a checked item | `tfc viewer/viewer.bas` |
| `syncfix/` | the acceptance program: a Syncthing conflict resolver (below), walking folder trees with Files() and Folders(), Name and FileCopy, Shell for diff, a multi-select ListBox | `tfc syncfix/syncfix.vbp` |
| `samples/` | one short program each for Select Case, Types and Refs, errors, arrays, strings and Dictionary, showing its results | `tfc samples/types.bas` |

`demo/demo.vbp` is also the program for checking a terminal: run it and
try the keys and the mouse. The address book keeps `addresses.txt` in the
folder it runs in.

Each example is also a run test (`tests/run/demo.vbp`,
`addressbook.vbp`, `viewer.vbp`, `syncfix.vbp`, `sample_*.vbp`), built from these files,
so none can stop working unnoticed.

## syncfix: resolving Syncthing conflicts

When two devices change a file before they have synced, Syncthing keeps
both: the losing version is renamed
`name.sync-conflict-YYYYMMDD-HHMMSS-DEVICE7.ext` (the time in UTC,
DEVICE7 the first seven characters of the ID of the device whose version
lost). syncfix finds these in every folder Syncthing's `config.xml`
lists, passing over `.stversions`, and shows each beside its original:

- **Keep original**: the conflict copy moves to the removed folder.
- **Keep conflict**: the original moves there, and the copy takes its
  name, so Syncthing sends it to every device.
- **Keep both**: the copy gets an ordinary name, such as
  `todo (tablet 2026-10-28).txt`, so the warning clears and the decision
  can wait.
- **Compare**: `diff -u` of the two through `less` (`fc` on Windows).

Nothing is deleted. Copies not kept go to
`~/sync-conflicts-removed/<today>/<folder label>/<path>`, with a
`log.txt` of what was done; the removed folder must be outside every
synced folder (syncfix refuses to resolve otherwise) and is best on the
same disk, where a move keeps the file's date. Device names come from
`config.xml`; a device it does not list is shown by its ID with a
warning, and resolving its conflicts asks first. Shift+arrows (or Space
and Ctrl+arrows) choose several conflicts to resolve together.

```
syncfix [--config FILE] [--removed FOLDER] [FOLDER...]
```

`config.xml` is looked for in `$STHOMEDIR`, `~/.local/state/syncthing`,
`~/.config/syncthing`, `~/Library/Application Support/Syncthing` and
(Windows) `%LOCALAPPDATA%\Syncthing`; `--config` gives another. Folders
named on the command line are searched instead of `config.xml`'s.

To run it on a Linux server, build it on the Mac and copy it over:

```
tfc --cc "zig cc -target x86_64-linux-musl" -o syncfix-linux syncfix/syncfix.vbp
scp syncfix-linux server:bin/syncfix
ssh -t server bin/syncfix
```
