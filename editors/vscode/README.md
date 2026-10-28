# TermForms BASIC for Visual Studio Code

Highlighting for `.bas` and `.vbp` files, breakpoints in `.bas` files, and
a problem matcher (`$tfc`) that puts tfc's errors and warnings in the
Problems list. Debugging goes through CodeLLDB with TermForms' lldb
printers.

## Install

The extension is not on the Marketplace yet; VS Code loads it from a
folder. Link this folder into VS Code's extensions folder, then restart
VS Code:

```sh
ln -s /path/to/termforms/editors/vscode ~/.vscode/extensions/termforms.termforms-basic
```

(Windows: `mklink /D "%USERPROFILE%\.vscode\extensions\termforms.termforms-basic" C:\path\to\termforms\editors\vscode`.)

For debugging, also install **CodeLLDB** (`vadimcn.vscode-lldb`) from the
Marketplace.

## Settings

- `termforms.tfc`: the compiler, if `tfc` is not on your PATH (e.g.
  `/path/to/termforms/build/dev/compiler/tfc`).
- `termforms.home`: the TermForms folder, so the debugger finds
  `debug/lldb/termforms.py`.

## Build and debug

Copy `templates/tasks.json` and `templates/launch.json` into your
program's `.vscode` folder (for a project, put its `.vbp` and program
name where they say `myprogram`).

- Cmd+Shift+B (Ctrl+Shift+B) builds the open file; errors are in the
  Problems list and underlined in the code.
- F9 sets a breakpoint on a `.bas` line; F5 builds and starts the
  program in a terminal of its own, stopping there. F5 uses the
  configuration chosen at the top of the Run and Debug view (Cmd+Shift+D):
  for a program of several files choose "TermForms: debug the project"
  (building one file of a project alone fails, as the others are
  missing), or delete the other entry.
- Local shows a Sub's own variables; module variables are under Static
  (with tfc's controls, c_<name>, and string literals, s_<n>); the Watch
  pane takes a name such as `g_total`.
- In the Variables pane a variable keeps its C name: `x` is `v_x` in a
  Sub, `g_x` at module level, and a Type's members are `f_<name>`.
  Strings, arrays and Dictionaries show as BASIC would; a Boolean shows
  as 0 or 1.
- Step Into goes into your Subs and Functions, past the runtime's own.

Windows: CodeLLDB needs the program's `.exe` in `program`; whether
Visual Studio's debugger suits zig-built programs is still to be checked.
