# TermForms

TermForms BASIC is a language with VB-DOS-style syntax for event-driven,
text-mode forms programs that run in a terminal, locally or over SSH.
`tfc` compiles a `.bas` program (or a `.vbp` project) to C, then has a C
compiler build it into a single program.

## Install

1. Unpack this folder wherever you like and add its `bin` to your PATH.
   tfc finds its runtime beside `bin`, wherever the folder is.
   - macOS / Linux: `export PATH="/path/to/termforms/bin:$PATH"` in your
     shell's profile (`~/.zprofile`, `~/.bashrc`).
   - Windows: Settings > System > About > Advanced system settings >
     Environment Variables > Path > New, and the folder's `bin`.
2. **macOS:** tfc is not signed. If you downloaded the archive with a
   browser, macOS marks it as quarantined, and unpacking passes the mark
   to every file, so macOS refuses to run `bin/tfc`. Clear it once, giving
   the unpacked folder (the one holding `bin`, `runtime` and `examples`):
   `xattr -dr com.apple.quarantine ~/termforms` (if you unpacked it
   to `~/termforms`). `-r` clears every file in it; `bin/tfc` is the only
   one that matters, so `xattr -d com.apple.quarantine ~/termforms/bin/tfc`
   does as well. Programs tfc builds are never quarantined. Downloading
   with `curl -LO` instead leaves no quarantine at all.
3. Install a C compiler, which tfc drives:
   - macOS: `xcode-select --install` (the Command Line Tools).
   - Linux: your distribution's gcc or clang (e.g. `sudo apt install gcc`).
   - Windows: `winget install zig.zig` (tfc uses `zig cc`; Visual Studio's
     `cl` works too, from a Developer Command Prompt).
     The first build with zig is slow (a minute or more on an older
     machine): zig builds its C library for Windows once and keeps it,
     and later builds are quick.

Check: `tfc --version`.

## First program

```sh
cd examples
tfc hello.bas
./hello            (Windows: hello.exe)
```

Tab moves between the controls, Enter presses the button, Ctrl+F4 closes
the form. `examples/README.md` lists the rest; `examples/demo` shows every
control, the menus, dialogs and the mouse.

## A program for a Linux server

zig builds for other machines. From a Mac or Windows:

```sh
tfc --cc "zig cc -target x86_64-linux-musl" -o hello-linux hello.bas
scp hello-linux server:
ssh -t server ./hello-linux
```

One static file, nothing to install on the server. From Windows the file
arrives without its executable bit: `chmod +x hello-linux` there once.

## Learning the language

- `docs/GUIDE.md`: the programmer's guide: start here.
- `docs/LANGUAGE.md`: what works today, briefly.
- `docs/REFERENCE.md`: every rule of the language, as decided.

## Editing and debugging

- `editors/termforms-basic-<version>.vsix`: a Visual Studio Code
  extension (highlighting, tfc's errors in the Problems list, breakpoints
  in `.bas` files), installed with VS Code's Install from VSIX.
  `editors/vscode` holds its files and the templates for building and
  debugging with CodeLLDB; its README says how to set it all up,
  Windows step by step.
- `debug/lldb/termforms.py` and `debug/gdb/termforms.py`: printers that
  show Strings, arrays and Dictionaries as BASIC would.

## Licences

tfc and the tools: BSD-2-Clause (`LICENSE`). The runtime, which goes into
every program you build: 0BSD (`runtime/LICENSE`), so your programs carry
no obligation. `runtime/THIRD_PARTY_NOTICES` has the Unicode data notice.
