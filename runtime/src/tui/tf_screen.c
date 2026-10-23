/* SPDX-License-Identifier: 0BSD */
/*
 * tf_screen.c -- the shadow-buffered screen.
 *
 * Drawing goes into the back buffer. A flush compares it with the front
 * buffer (what the terminal is known to show) and sends only the cells that
 * differ, which keeps updates small over slow or high-latency SSH links.
 */
#include "tf_tui.h"

#include <stdlib.h>
#include <string.h>

typedef struct tf_cell
{
    uint32_t ch;
    uint8_t fg;
    uint8_t bg;
} tf_cell;

/* A front-buffer value that never matches a real cell, forcing a redraw. */
#define TF_CELL_INVALID 0xFFFFFFFFu

static tf_cell *back = NULL;
static tf_cell *front = NULL;
static int width = 0;
static int height = 0;
static int terminal = 0;
static int cursor_x = 0;
static int cursor_y = 0;
static int cursor_visible = 0;

/* Output is assembled here and written in one call per flush. */
static char *out = NULL;
static size_t out_length = 0;
static size_t out_capacity = 0;

static void emit(const char *bytes, size_t length)
{
    if (out_length + length > out_capacity)
    {
        size_t capacity = out_capacity == 0 ? 4096 : out_capacity;
        while (capacity < out_length + length)
        {
            capacity *= 2;
        }
        out = tf_xrealloc(out, capacity);
        out_capacity = capacity;
    }
    memcpy(out + out_length, bytes, length);
    out_length += length;
}

static void emitf_move(int x, int y)
{
    char sequence[32];
    const int n = snprintf(sequence, sizeof sequence, "\x1b[%d;%dH", y + 1, x + 1);
    emit(sequence, (size_t)n);
}

static void emit_colours(int fg, int bg)
{
    /* 30-37 / 90-97 foreground, 40-47 / 100-107 background. */
    char sequence[32];
    const int f = fg < 8 ? 30 + fg : 90 + fg - 8;
    const int b = bg < 8 ? 40 + bg : 100 + bg - 8;
    const int n = snprintf(sequence, sizeof sequence, "\x1b[0;%d;%dm", f, b);
    emit(sequence, (size_t)n);
}

int tf_utf8_decode(const char *s, int length, uint32_t *cp)
{
    const unsigned char *u = (const unsigned char *)s;
    int needed;
    uint32_t value;

    if (u[0] < 0x80)
    {
        *cp = u[0];
        return 1;
    }
    if (u[0] >= 0xF0 && u[0] < 0xF8)
    {
        needed = 4;
        value = u[0] & 0x07u;
    }
    else if (u[0] >= 0xE0)
    {
        needed = 3;
        value = u[0] & 0x0Fu;
    }
    else if (u[0] >= 0xC0)
    {
        needed = 2;
        value = u[0] & 0x1Fu;
    }
    else
    {
        *cp = 0xFFFD; /* stray continuation byte */
        return 1;
    }

    if (length < needed)
    {
        *cp = 0xFFFD;
        return length > 0 ? length : 1;
    }
    for (int i = 1; i < needed; ++i)
    {
        if ((u[i] & 0xC0u) != 0x80u)
        {
            *cp = 0xFFFD;
            return i;
        }
        value = (value << 6) | (u[i] & 0x3Fu);
    }
    *cp = value;
    return needed;
}

int tf_utf8_encode(uint32_t cp, char out_bytes[4])
{
    if (cp < 0x80)
    {
        out_bytes[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800)
    {
        out_bytes[0] = (char)(0xC0 | (cp >> 6));
        out_bytes[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000)
    {
        out_bytes[0] = (char)(0xE0 | (cp >> 12));
        out_bytes[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out_bytes[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out_bytes[0] = (char)(0xF0 | ((cp >> 18) & 0x07));
    out_bytes[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out_bytes[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out_bytes[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

void tf_screen_init(int cols, int rows, int to_terminal)
{
    const size_t cells = (size_t)cols * (size_t)rows;
    tf_screen_free();
    back = tf_xcalloc(cells, sizeof(tf_cell));
    front = tf_xcalloc(cells, sizeof(tf_cell));
    width = cols;
    height = rows;
    terminal = to_terminal;
    tf_screen_fill(0, 0, cols, rows, ' ', TF_GREY, TF_BLACK);
    tf_screen_invalidate();
}

void tf_screen_free(void)
{
    free(back);
    free(front);
    free(out);
    back = front = NULL;
    out = NULL;
    out_length = out_capacity = 0;
    width = height = 0;
}

void tf_screen_size(int *cols, int *rows)
{
    *cols = width;
    *rows = height;
}

void tf_screen_invalidate(void)
{
    for (int i = 0; i < width * height; ++i)
    {
        front[i].ch = TF_CELL_INVALID;
    }
}

void tf_screen_put(int x, int y, uint32_t ch, int fg, int bg)
{
    if (x >= 0 && y >= 0 && x < width && y < height)
    {
        tf_cell *cell = &back[y * width + x];
        cell->ch = ch;
        cell->fg = (uint8_t)fg;
        cell->bg = (uint8_t)bg;
    }
}

void tf_screen_fill(int x, int y, int w, int h, uint32_t ch, int fg, int bg)
{
    for (int row = y; row < y + h; ++row)
    {
        for (int col = x; col < x + w; ++col)
        {
            tf_screen_put(col, row, ch, fg, bg);
        }
    }
}

int tf_screen_text(int x, int y, const char *utf8, int max_cells, int fg, int bg)
{
    /* Every code point takes one cell here; wide characters come later with
     * the Unicode width tables. */
    int cells = 0;
    int length = (int)strlen(utf8);
    while (length > 0 && cells < max_cells)
    {
        uint32_t cp = 0;
        const int used = tf_utf8_decode(utf8, length, &cp);
        tf_screen_put(x + cells, y, cp, fg, bg);
        utf8 += used;
        length -= used;
        cells++;
    }
    return cells;
}

void tf_screen_cursor(int x, int y, int visible)
{
    cursor_x = x;
    cursor_y = y;
    cursor_visible = visible;
}

void tf_screen_flush(void)
{
    if (!terminal)
    {
        return;
    }

    out_length = 0;
    emit("\x1b[?25l", 6); /* hide the cursor while drawing */

    int last_x = -1;
    int last_y = -1;
    int last_fg = -1;
    int last_bg = -1;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const tf_cell *b = &back[y * width + x];
            tf_cell *f = &front[y * width + x];
            if (b->ch == f->ch && b->fg == f->fg && b->bg == f->bg)
            {
                continue;
            }
            if (x != last_x || y != last_y)
            {
                emitf_move(x, y);
            }
            if (b->fg != last_fg || b->bg != last_bg)
            {
                emit_colours(b->fg, b->bg);
                last_fg = b->fg;
                last_bg = b->bg;
            }
            char encoded[4];
            emit(encoded, (size_t)tf_utf8_encode(b->ch, encoded));
            *f = *b;
            last_x = x + 1;
            last_y = y;
        }
    }

    if (cursor_visible)
    {
        emitf_move(cursor_x, cursor_y);
        emit("\x1b[?25h", 6);
    }
    tf_term_write(out, out_length);
}

void tf_screen_dump(FILE *stream)
{
    char line[4096];
    for (int y = 0; y < height; ++y)
    {
        size_t used = 0;
        size_t keep = 0; /* length up to the last non-space character */
        for (int x = 0; x < width; ++x)
        {
            char encoded[4];
            const int n = tf_utf8_encode(back[y * width + x].ch, encoded);
            if (used + (size_t)n >= sizeof line)
            {
                break;
            }
            memcpy(line + used, encoded, (size_t)n);
            used += (size_t)n;
            if (back[y * width + x].ch != ' ')
            {
                keep = used;
            }
        }
        fwrite(line, 1, keep, stream);
        fputc('\n', stream);
    }
}
