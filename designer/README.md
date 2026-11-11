# tfdesign: the TermForms form designer

A visual designer for TermForms forms, written in TermForms BASIC. It runs
in the terminal like any TermForms program, over SSH too.

```sh
tfc designer.vbp            # builds ./tfdesign
./tfdesign ../examples/hello.bas
```

Give it a form's `.bas` or `.frm`. A name that does not exist yet starts a
new form. The designer needs a terminal of at least 90 x 24 cells and
fills whatever it is given.

## What it writes

- **The form block, always the same way:** controls in tab order,
  properties in the catalog's order, only values that differ from the
  defaults. Comments in the block are kept with the line after them.
  The first save of a block written by hand may therefore reformat it
  (aligned `=` signs, default values dropped).
- **Where the block lives:** a new form goes in a `.frm` beside the
  `.bas`, which tfc reads as that module's form. A block already in a
  `.bas` is edited where it is; File > Move form to .frm takes it out.
- **Event handlers:** double-click a control (or press Enter on an event
  in the Events list) to write an empty `Sub` for it at the end of the
  `.bas`. One already there is not written again. The status line says
  where it is.
- **Renaming a control** renames its handlers too: the empty ones at
  once, those with code in them if you agree. One whose new name is
  already taken is left. Other uses of the old name in the code are not
  changed; the status line counts them, and tfc reports them at the next
  build. The handlers are renamed in the `.bas` straight away, so Undo
  brings back the control's old name but not theirs.

Before saving, the designer checks whether the file changed on disk since
it was read. The `.bas` is read again so the code around the block stays
as it stands. Every file is written to a new file first and then put in
place, so it is never left half written.

## Keys

| Where | Keys |
| --- | --- |
| On the form | click selects; drag moves; drag the handle ◢ to size; arrows move, Shift+arrows size; Del deletes; `[` and `]` step through the controls; Enter goes to the properties |
| Toolbox | choose a control, then click on the form to place it (into a Frame if clicked inside one); Enter puts it at the top left |
| Properties | Enter or a double-click edits the value below; Enter applies it, Esc leaves it |
| Anywhere | Ctrl+S save, Ctrl+Z undo, Ctrl+Y redo, F6 tab order, F7 the code, F1 keys, Ctrl+Q quit |

Dropping a control into a Frame puts it in the Frame; dragging it out
takes it out. In tab-order mode (F6), click the controls in the order
wanted; Esc or F6 ends it.

F7 opens the `.bas` at the selected control's first handler in
`$VISUAL` (else `$EDITOR`), which is how git chooses an editor. Without
either, the status line gives the file and line.

## How it finds tfc

The designer asks `tfc --catalog` which controls, properties and events
there are, so it always matches the compiler. It uses `$TFC` if set, else
a `tfc` beside the designer, else the `tfc` on the PATH.
