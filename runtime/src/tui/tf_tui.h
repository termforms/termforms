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
    TF_KEY_MOUSE, /* the left button or the wheel: .ch a TF_MOUSE_ action, at .x, .y */
    TF_KEY_RESIZE,
    TF_KEY_EOF /* headless script exhausted */
} tf_key_kind;

/* Modifiers, for function keys (as xterm reports them: 1 + this). */
#define TF_MOD_SHIFT 1
#define TF_MOD_ALT 2
#define TF_MOD_CTRL 4

/* What a TF_KEY_MOUSE did (the left button only; the others are ignored). */
#define TF_MOUSE_PRESS 1
#define TF_MOUSE_RELEASE 2
#define TF_MOUSE_DRAG 3 /* moved with the button down */
#define TF_MOUSE_WHEEL_UP 4
#define TF_MOUSE_WHEEL_DOWN 5

typedef struct tf_key
{
    tf_key_kind kind;
    uint32_t ch;
    int mods;
    int x, y; /* TF_KEY_MOUSE: the cell, from 0 at the top left */
} tf_key;

/* Byte-stream decoder state. */
typedef struct tf_keys
{
    unsigned char pending[64];
    int count;
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

/* The 16 standard terminal colours. */
enum
{
    TF_BLACK = 0,
    TF_RED = 1,
    TF_GREEN = 2,
    TF_YELLOW = 3,
    TF_BLUE = 4,
    TF_MAGENTA = 5,
    TF_CYAN = 6,
    TF_GREY = 7,
    TF_BRIGHT = 8, /* add to any of the above */
    TF_WHITE = 15
};

void tf_screen_init(int cols, int rows, int to_terminal);
void tf_screen_free(void);
void tf_screen_size(int *cols, int *rows);
void tf_screen_fill(int x, int y, int width, int height, uint32_t ch, int fg, int bg);
void tf_screen_put(int x, int y, uint32_t ch, int fg, int bg);

/* Writes UTF-8 text from (x, y), at most max_cells wide. Returns cells used. */
int tf_screen_text(int x, int y, const char *utf8, int max_cells, int fg, int bg);

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

void tf_edit_free(tf_edit *e);

/* ------------------------------------------------------------------------ */
/* Input                                                                    */
/* ------------------------------------------------------------------------ */

/* Starts input from the terminal, or from the script when headless. */
void tf_input_start(const char *headless_script);

/* Waits for the next key. */
tf_key tf_input_next(void);

/* As tf_input_next, but gives TF_KEY_NONE once timeout_ms pass with no
   key (-1: wait for ever). A key script never times out. */
tf_key tf_input_wait(int timeout_ms);

#endif /* TERMFORMS_TF_TUI_H */
