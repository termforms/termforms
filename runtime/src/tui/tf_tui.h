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
    TF_KEY_ESC,
    TF_KEY_ALT, /* Alt+key (ESC then the key); .ch is the character, letters lower-case */
    TF_KEY_F,   /* function key; .ch is its number, 1-12; .mods as pressed */
    TF_KEY_RESIZE,
    TF_KEY_EOF /* headless script exhausted */
} tf_key_kind;

/* Modifiers, for function keys (as xterm reports them: 1 + this). */
#define TF_MOD_SHIFT 1
#define TF_MOD_ALT 2
#define TF_MOD_CTRL 4

typedef struct tf_key
{
    tf_key_kind kind;
    uint32_t ch;
    int mods;
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

/* Decodes one UTF-8 code point; returns bytes consumed (at least 1). */
int tf_utf8_decode(const char *s, int length, uint32_t *cp);

/* Encodes one code point; returns bytes written (1 to 4). */
int tf_utf8_encode(uint32_t cp, char out[4]);

/* ------------------------------------------------------------------------ */
/* Input                                                                    */
/* ------------------------------------------------------------------------ */

/* Starts input from the terminal, or from the script when headless. */
void tf_input_start(const char *headless_script);

/* Waits for the next key. */
tf_key tf_input_next(void);

#endif /* TERMFORMS_TF_TUI_H */
