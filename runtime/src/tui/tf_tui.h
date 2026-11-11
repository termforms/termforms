/* SPDX-License-Identifier: 0BSD */
/*
 * tf_tui.h -- internal interfaces of the text user interface.
 *
 *   terminal  platform layer: raw mode, alternate screen, byte I/O, size
 *             (platform/posix/tf_term_posix.c, platform/win32/tf_term_win32.c)
 *   keys      turns input bytes into key events (tf_keys.c)
 *   screen    shadow-buffered cell grid; sends only changed cells (tf_screen.c)
 *   input     key source: the terminal, or a script in headless mode (tf_input.c)
 *   forms     controls, rendering and the event loop (tf_forms.c)
 *
 * Headless mode: when the environment variable TERMFORMS_HEADLESS is set, its
 * value is a key script (see tf_input.c) and no terminal is touched. When the
 * form closes, the final screen is printed to stdout as text. The test suite
 * uses this to run real programs on CI machines that have no terminal.
 */
#ifndef TERMFORMS_TF_TUI_H
#define TERMFORMS_TF_TUI_H

#include "../tf_internal.h"

#include <stddef.h>
#include <stdio.h>

/* ------------------------------------------------------------------------ */
/* Terminal (platform layer)                                                */
/* ------------------------------------------------------------------------ */

/* Enters raw mode and the alternate screen. Returns 0, or -1 with a reason. */
int tf_term_open(const char **reason);

/* Restores the terminal exactly as it was found. Safe to call twice. */
void tf_term_close(void);

void tf_term_write(const char *bytes, size_t length);

/*
 * Reads available input bytes, waiting up to timeout_ms (-1 waits forever).
 * Returns the number of bytes read, or 0 on timeout, resize or interruption.
 */
int tf_term_read(unsigned char *buffer, int capacity, int timeout_ms);

void tf_term_size(int *cols, int *rows);

/* Returns 1 once after each terminal resize, otherwise 0. */
int tf_term_take_resize(void);

/* Whether box-drawing characters can be sent as UTF-8. */
int tf_term_utf8(void);

/* tf_platform_getenv is declared in tf_internal.h (Environ$ uses it too). */

/* ------------------------------------------------------------------------ */
/* Keys                                                                     */
/* ------------------------------------------------------------------------ */

typedef enum tf_key_kind
{
    TF_KEY_NONE,
    TF_KEY_CHAR, /* printable code point in .ch */
    TF_KEY_CTRL, /* Ctrl+letter; .ch is the lower-case letter */
    TF_KEY_ENTER,
    TF_KEY_TAB,
    TF_KEY_BACKTAB,
    TF_KEY_BACKSPACE,
    TF_KEY_DELETE,
    TF_KEY_LEFT,
    TF_KEY_RIGHT,
    TF_KEY_UP,
    TF_KEY_DOWN,
    TF_KEY_HOME,
    TF_KEY_END,
    TF_KEY_PGUP,
    TF_KEY_PGDN,
    TF_KEY_ESC,
    TF_KEY_ALT,   /* Alt+key (ESC then the key); .ch is the character, letters lower-case */
    TF_KEY_F,     /* function key; .ch is its number, 1-12; .mods as pressed */
    TF_KEY_WAIT,  /* key scripts only: {WAIT n}, n ms in .ch, for Timers */
    TF_KEY_MOUSE, /* a button or the wheel: .ch a TF_MOUSE_ action, at .x, .y */
    TF_KEY_RESIZE,
    TF_KEY_EOF /* headless script exhausted */
} tf_key_kind;

/* Modifiers, for function keys (as xterm reports them: 1 + this). */
#define TF_MOD_SHIFT 1
#define TF_MOD_ALT 2
#define TF_MOD_CTRL 4

/* What a TF_KEY_MOUSE did; a press, drag or release also says which button
   (.button, VB's values: TF_BUTTON_LEFT and so on). */
#define TF_MOUSE_PRESS 1
#define TF_MOUSE_RELEASE 2
#define TF_MOUSE_DRAG 3 /* moved with the button down */
#define TF_MOUSE_WHEEL_UP 4
#define TF_MOUSE_WHEEL_DOWN 5

#define TF_BUTTON_LEFT 1
#define TF_BUTTON_RIGHT 2
#define TF_BUTTON_MIDDLE 4

typedef struct tf_key
{
    tf_key_kind kind;
    uint32_t ch;
    int mods;
    int x, y;   /* TF_KEY_MOUSE: the cell, from 0 at the top left */
    int button; /* TF_KEY_MOUSE: TF_BUTTON_ (0 for the wheel) */
} tf_key;

/* Byte-stream decoder state. */
typedef struct tf_keys
{
    unsigned char pending[64];
    int count;
    int pasting;  /* inside a bracketed paste: text, not keys */
    int after_cr; /* the paste's last byte was CR (so a LF after it is one break) */
} tf_keys;

void tf_keys_feed(tf_keys *keys, const unsigned char *bytes, int length);

/*
 * Takes the next complete key from the buffer. Returns 1 if a key was
 * produced. A lone ESC is ambiguous (it also starts sequences), so it is only
 * reported when flush_escape is set, i.e. after a short quiet period.
 */
int tf_keys_next(tf_keys *keys, tf_key *key, int flush_escape);

/* ------------------------------------------------------------------------ */
/* Screen                                                                   */
/* ------------------------------------------------------------------------ */

/* Colours are exact, 0xRRGGBB (see tf_colour.c). A foreground may also
   carry TF_UNDERLINE, as an access key does. */
#define TF_UNDERLINE 0x01000000u

/* The colour a role is drawn in now (its default, or the project's). */
uint32_t tf_colour(tf_colour_role role);

/* The SGR sequence that selects a foreground and background, written into
   buffer (at least 64 bytes): exact colours, or the nearest of the 256 or
   16 the terminal has when TERMFORMS_COLOR says it has no more. Returns
   its length. */
int tf_colour_sgr(uint32_t fg, uint32_t bg, char *buffer, size_t size);

/* The same for a given depth: 24 (exact), 256 or 16 (tests use it). */
int tf_colour_sgr_at(uint32_t fg, uint32_t bg, int bits, char *buffer, size_t size);

void tf_screen_init(int cols, int rows, int to_terminal);
void tf_screen_free(void);
void tf_screen_size(int *cols, int *rows);
void tf_screen_fill(int x, int y, int width, int height, uint32_t ch, uint32_t fg, uint32_t bg);
void tf_screen_put(int x, int y, uint32_t ch, uint32_t fg, uint32_t bg);
/* Writes go only inside this rectangle until tf_screen_unclip (a FormView
   drawing the form it shows); colours a rectangle, keeping its characters
   (and their underlines). */
void tf_screen_clip(int x, int y, int width, int height);
void tf_screen_unclip(void);
void tf_screen_paint(int x, int y, int width, int height, uint32_t fg, uint32_t bg);

/* Writes UTF-8 text from (x, y), at most max_cells wide. Returns cells used. */
int tf_screen_text(int x, int y, const char *utf8, int max_cells, uint32_t fg, uint32_t bg);

void tf_screen_cursor(int x, int y, int visible);

/* Sends changed cells to the terminal (does nothing in headless mode). */
void tf_screen_flush(void);

/* Forces the next flush to redraw everything (after a resize). */
void tf_screen_invalidate(void);

/* Prints the screen as plain text, one line per row, trailing spaces cut. */
void tf_screen_dump(FILE *out);

/* Cells a character takes on a terminal: 2 wide (CJK, emoji), 0 for one
   drawn with the character before it (a combining mark) or not at all
   (controls, format characters), else 1 (tf_width.c). */
int tf_char_width(uint32_t cp);

/* Cells UTF-8 text takes: the sum of its characters' widths. */
int tf_text_cells(const char *utf8);

/* Decodes one UTF-8 code point; returns bytes consumed (at least 1). */
int tf_utf8_decode(const char *s, int length, uint32_t *cp);

/* Encodes one code point; returns bytes written (1 to 4). */
int tf_utf8_encode(uint32_t cp, char out[4]);

/* ------------------------------------------------------------------------ */
/* Edit buffer (tf_edit.c): a TextBox's text, caret and selection          */
/* ------------------------------------------------------------------------ */

typedef struct tf_edit
{
    tf_string *text;    /* NULL is "" */
    int32_t cursor;     /* the caret, a byte offset */
    int32_t anchor;     /* the selection's other end (== cursor: none) */
    int32_t max_length; /* code points the user may type; 0 no limit */
    int locked;         /* the user may select and copy but not change it */
    int concealed;      /* a password: no Cut or Copy */
    tf_string *undo;    /* the text before the last change (Ctrl+Z) */
    int32_t undo_cursor;
    int has_undo;
    int typing; /* the last change was typing (more typing joins it) */

    /* A multi-line TextBox: Enter starts a new line and the caret moves
     * by rows. Rows are laid out from the text whenever they are needed
     * (`wrap` cells wide, 0 for no wrapping; a tab to the next multiple
     * of 8); `page` is the rows shown, for PgUp and PgDn; `goal` the
     * column (in cells) Up and Down keep to, while `has_goal`. */
    int multi_line;
    int wrap;
    int page;
    int goal;
    int has_goal;
    /* Bumped by every change of the text, caret or selection, so drawing
       knows when to bring the caret into view. */
    uint32_t version;
} tf_edit;

/* What tf_edit_key did with a key. */
#define TF_EDIT_UNUSED 0  /* not an editing key */
#define TF_EDIT_MOVED 1   /* used: caret or selection, or nothing to do */
#define TF_EDIT_CHANGED 2 /* the text changed */

int tf_edit_key(tf_edit *e, tf_key key);

/* The program's changes: Text (caret to the end, no undo), SelStart and
   SelLength (code points, clamped), SelText. */
void tf_edit_set_text(tf_edit *e, const tf_string *text);
void tf_edit_select(tf_edit *e, int32_t start, int32_t length);
void tf_edit_replace_selection(tf_edit *e, const tf_string *text);

/* The mouse: the caret to code point cp (extend: selecting from the
   anchor); a double-click selects the word at cp. */
void tf_edit_place(tf_edit *e, int32_t cp, int extend);
void tf_edit_select_word(tf_edit *e, int32_t cp);
int32_t tf_edit_sel_start(const tf_edit *e);
int32_t tf_edit_sel_length(const tf_edit *e);
tf_string *tf_edit_sel_text(const tf_edit *e); /* owned */

/* For drawing: the caret's code point index; whether code point cp is
   selected. */
int32_t tf_edit_cursor_index(const tf_edit *e);
int tf_edit_selected(const tf_edit *e, int32_t cp);

/* Rows (multi-line): the row starting at byte `start` ends at the byte
   returned; *next is where the following row starts, -1 after the last.
   A row ends before its line break, or where it wraps (after the last
   space that fits, else after the last character that fits). */
int32_t tf_edit_row_end(const tf_edit *e, int32_t start, int32_t *next);
/* The cells from a row's start to byte `to` in it, tabs expanded. */
int tf_edit_cells(const tf_edit *e, int32_t start, int32_t to);
/* The row byte `at` is on, from 0, and its start. */
int32_t tf_edit_row_of(const tf_edit *e, int32_t at, int32_t *start);
/* The byte nearest column `cells` on row `row` (clamped to the rows). */
int32_t tf_edit_offset_at(const tf_edit *e, int32_t row, int cells);
/* The mouse on a multi-line box: the caret to a byte offset (extend:
   selecting from the anchor), or the word there selected. */
void tf_edit_place_at(tf_edit *e, int32_t at, int extend);
void tf_edit_select_word_at(tf_edit *e, int32_t at);
/* The caret and the selection as byte offsets, for drawing. */
int32_t tf_edit_caret(const tf_edit *e);
void tf_edit_sel_bytes(const tf_edit *e, int32_t *from, int32_t *to);
/* Text the program gives a multi-line box: CR LF and lone CRs become LF.
   Owned. */
tf_string *tf_edit_lines_of(const tf_string *text);

void tf_edit_free(tf_edit *e);

/* Text the user copied or cut (Ctrl+C, Ctrl+X), offered to the system's
   clipboard as well as kept as the program's own (tf_forms.c). */
void tf_clipboard_export(const char *bytes, int32_t length);

/* n bytes as base64 (for OSC 52) into out, which has room for
   (n + 2) / 3 * 4; returns the characters written. */
size_t tf_base64(const char *bytes, size_t n, char *out);

/* ------------------------------------------------------------------------ */
/* Input                                                                    */
/* ------------------------------------------------------------------------ */

/* Whether the program takes the mouse: yes unless TERMFORMS_MOUSE is 0
   (for a terminal where the mouse is better left to selecting text). */
int tf_mouse_wanted(void);

/* Starts input from the terminal, or from the script when headless. */
void tf_input_start(const char *headless_script);

/* Waits for the next key. */
tf_key tf_input_next(void);

/* As tf_input_next, but gives TF_KEY_NONE once timeout_ms pass with no
   key (-1: wait for ever). A key script never times out. */
tf_key tf_input_wait(int timeout_ms);

#endif /* TERMFORMS_TF_TUI_H */
