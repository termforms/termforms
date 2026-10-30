# TermForms BASIC for Visual Studio Code

Highlighting for `.bas` and `.vbp` files, breakpoints in `.bas` files, and
a problem matcher (`$tfc`) that puts tfc's errors and warnings in the
Problems list. Debugging goes through CodeLLDB with TermForms' lldb
printers.

## Install

Before starting VS Code, put TermForms' `bin` on your PATH (see the
release's README). VS Code reads PATH only when it starts; after changing
PATH, quit VS Code completely and start it again, or its build task cannot
find `tfc`.

1. In VS Code, open the Extensions view (Cmd+Shift+X; Windows and Linux
   Ctrl+Shift+X). Install **CodeLLDB** (`vadimcn.vscode-lldb`), the
   debugger.
2. In the same view, the `...` menu at its top has **Install from
   VSIX...**: choose `editors/termforms-basic-<version>.vsix` in the
   TermForms folder. Install a newer release's the same way; it replaces
   the old.
3. Open Settings (Cmd+, or Ctrl+,), type `termforms`, and set **Termforms:
   Home** to the TermForms folder (the one holding `bin`, `runtime` and
   `debug`), so the debugger finds its printers. Leave **Termforms: Tfc**
   as `tfc` while `bin` is on PATH. If searching finds nothing, the
   extension is not installed.
4. Add the keyboard shortcuts below, so that Ctrl+Q and Ctrl+B reach your
   program.

If you once linked or copied this folder into VS Code's extensions
folder by hand, delete that `termforms.termforms-basic` folder first
(`~/.vscode/extensions`, on Windows `%USERPROFILE%\.vscode\extensions`).

Working from a source checkout instead of a release, link this folder in
its place and restart VS Code:

```sh
ln -s /path/to/termforms/editors/vscode ~/.vscode/extensions/termforms.termforms-basic
```

## Keys that VS Code keeps

A program run in VS Code's terminal panel gets the keys typed there, but
VS Code takes some first: Ctrl+Q opens its Quick Open View, and on Windows
and Linux Ctrl+B shows or hides its sidebar. A program's menus use both
(Ctrl+B is F10's alternative, and Ctrl+Q is a common Shortcut for Exit).
To pass them to the program while the terminal has the focus, press
Cmd+Shift+P (Ctrl+Shift+P), choose **Preferences: Open Keyboard Shortcuts
(JSON)**, and put these between its `[` and `]` (with a comma after any
entry already there):

```json
{
    "key": "ctrl+q",
    "command": "workbench.action.terminal.sendSequence",
    "args": { "text": "\u0011" },
    "when": "terminalFocus"
},
{
    "key": "ctrl+b",
    "command": "workbench.action.terminal.sendSequence",
    "args": { "text": "\u0002" },
    "when": "terminalFocus"
}
```

(`\u0011` and `\u0002` are the characters a terminal sends for Ctrl+Q and
Ctrl+B.) They take effect when saved. Elsewhere in VS Code the keys keep
their usual meaning.

Close a form with **Ctrl+F4**. Alt+F4 never reaches a program in VS Code
on Windows: Windows closes VS Code's own window with it.

## Settings

- `termforms.tfc`: the compiler, if `tfc` is not on your PATH (e.g.
  `/path/to/termforms/build/dev/compiler/tfc`).
- `termforms.home`: the TermForms folder, so the debugger finds
  `debug/lldb/termforms.py`. The Settings screen takes a path as it is.
  On Windows, if you type it into `settings.json` by hand instead, use
  forward slashes, `C:/Users/me/termforms` (or double each backslash):
  JSON reads `\t` in `C:\Users\me\termforms` as a tab.

## Build and debug

Open your program's folder (File > Open Folder...) and make a `.vscode`
folder in it. Copy `templates/tasks.json` and `templates/launch.json`
into it from `editors/vscode` in the TermForms folder (for a project, put
its `.vbp` and program name where they say `myprogram`).

- Cmd+Shift+B (Ctrl+Shift+B) builds the open file; errors are in the
  Problems list and underlined in the code.
- F9 sets a breakpoint on a `.bas` line; F5 builds and starts the
  program in VS Code's terminal panel, stopping there. F5 uses the
  configuration chosen at the top of the Run and Debug view (Cmd+Shift+D):
  for a program of several files choose "TermForms: debug the project"
  (building one file of a project alone fails, as the others are
  missing), or delete the other entry. "Debug this file" builds the file
  open in the editor, so click into it before pressing F5.
- Click into the terminal panel before typing to the program; while it is
  stopped at a breakpoint, the editor has the focus and F10 steps.
- Local shows a Sub's own variables; module variables are under Static
  (with tfc's controls, c_<name>, and string literals, s_<n>); the Watch
  pane takes a name such as `g_total`.
- In the Variables pane a variable keeps its C name: `x` is `v_x` in a
  Sub, `g_x` at module level, and a Type's members are `f_<name>`.
  Strings, arrays and Dictionaries show as BASIC would; a Boolean shows
  as 0 or 1.
- Step Into goes into your Subs and Functions, past the runtime's own.

## Windows, step by step

Tried on a clean Windows 11 laptop (TermForms 0.9.1, with the fixes that
0.9.2 brings). The
paths below assume TermForms unpacked to `C:\Users\me\termforms`.

1. Set up TermForms from the release's README: unpack the zip, add
   `C:\Users\me\termforms\bin` to PATH, `winget install zig.zig`, and
   check that `tfc --version` works in a new PowerShell window.
2. Install VS Code (`winget install Microsoft.VisualStudioCode`, or from
   code.visualstudio.com). If it was already running, quit it and start
   it again, so it sees the new PATH.
3. Install CodeLLDB and `editors\termforms-basic-<version>.vsix`, set
   Termforms: Home to `C:\Users\me\termforms`, and add the two keyboard
   shortcuts, as under Install above.
4. Open `C:\Users\me\termforms\examples` as a folder (trust its authors
   when asked), make `examples\.vscode`, and copy both templates from
   `editors\vscode\templates` into it.
5. Open `hello.bas`: its keywords are coloured and the status bar says
   TermForms BASIC. Ctrl+Shift+B builds it, writing `hello.exe` and
   `hello.pdb` (the debug information, which zig puts in a file of its
   own: keep the two together).
6. Click line 31, `greetings = greetings + 1`, and press F9. In Run and
   Debug choose "TermForms: debug this file", click back into `hello.bas`
   and press F5. The form appears in the terminal panel; click into it,
   type a name, Tab to the button and press Enter. VS Code stops on line
   31 and Local shows `v_who` as the name, in quotes. F10 steps, F5 goes
   on, and Ctrl+F4 closes the form, ending the session.

`launch.json` names the program with `.exe` on Windows (its `windows`
entries). Whether Visual Studio's own debugger suits these programs is
still to be checked; the `.pdb` is the format it reads.
