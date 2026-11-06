/* SPDX-License-Identifier: 0BSD */
/*
 * tf_colour.c -- the colours forms are drawn in.
 *
 * Forms are drawn in exact colours (SGR 38;2 and 48;2), never in the
 * terminal's sixteen named ones: a colour scheme may make those anything,
 * and under some (Dracula's, and Jim's own profile) white and grey or blue
 * and white came out alike, so captions and access keys vanished. Exact
 * colours look the same in every terminal (Jim, 6 Nov 2026). The defaults
 * are the DOS look: blue forms with white text, grey menus and dialogs
 * with black text and red access keys, the focus black on cyan, on a dark
 * grey backdrop; the VGA palette's own values where it has one.
 *
 * Every terminal TermForms supports takes exact colours (Windows Terminal,
 * conhost, iTerm2, Ghostty, Terminal.app from macOS 26, VS Code). For one
 * that does not (tmux without its RGB setting, older terminals), the user
 * sets TERMFORMS_COLOR=256 or 16 and gets the nearest colour of those it
 * has. That is the terminal's business, so it is the user's to set; how a
 * program looks is the programmer's, in the project file.
 */
#include "tf_tui.h"

#include <string.h>

static uint32_t palette[TF_COLOUR_COUNT] = {
    [TF_COLOUR_BACKDROP] = 0x303030,       [TF_COLOUR_FORM_BACK] = 0x0000AA,
    [TF_COLOUR_FORM_TEXT] = 0xFFFFFF,      [TF_COLOUR_FORM_DISABLED] = 0xAAAAAA,
    [TF_COLOUR_FORM_KEY] = 0xFFFF55,       [TF_COLOUR_DIALOG_BACK] = 0xAAAAAA,
    [TF_COLOUR_DIALOG_TEXT] = 0x000000,    [TF_COLOUR_DIALOG_DISABLED] = 0x555555,
    [TF_COLOUR_DIALOG_KEY] = 0xAA0000,     [TF_COLOUR_MENU_BACK] = 0xAAAAAA,
    [TF_COLOUR_MENU_TEXT] = 0x000000,      [TF_COLOUR_MENU_DISABLED] = 0x555555,
    [TF_COLOUR_MENU_KEY] = 0xAA0000,       [TF_COLOUR_BUTTON_BACK] = 0xAAAAAA,
    [TF_COLOUR_BUTTON_TEXT] = 0x000000,    [TF_COLOUR_BUTTON_DISABLED] = 0x555555,
    [TF_COLOUR_BUTTON_KEY] = 0xAA0000,     [TF_COLOUR_HIGHLIGHT_BACK] = 0x00AAAA,
    [TF_COLOUR_HIGHLIGHT_TEXT] = 0x000000, [TF_COLOUR_INACTIVE_BACK] = 0x555555,
    [TF_COLOUR_INACTIVE_TEXT] = 0xFFFFFF,  [TF_COLOUR_FIELD_BACK] = 0x000000,
    [TF_COLOUR_FIELD_TEXT] = 0xFFFFFF,
};

void tf_colour_set(tf_colour_role role, int32_t rgb)
{
    if ((int)role >= 0 && role < TF_COLOUR_COUNT)
    {
        palette[role] = (uint32_t)rgb & 0xFFFFFFu;
    }
}

uint32_t tf_colour(tf_colour_role role)
{
    return palette[role];
}

/* 24 (exact), 256 or 16: what TERMFORMS_COLOR says the terminal has. */
static int depth(void)
{
    static int known = 0;
    if (known == 0)
    {
        const char *setting = tf_platform_getenv("TERMFORMS_COLOR");
        known = setting != NULL && strcmp(setting, "256") == 0  ? 256
                : setting != NULL && strcmp(setting, "16") == 0 ? 16
                                                                : 24;
    }
    return known;
}

static int red(uint32_t c)
{
    return (int)((c >> 16) & 0xFF);
}

static int green(uint32_t c)
{
    return (int)((c >> 8) & 0xFF);
}

static int blue(uint32_t c)
{
    return (int)(c & 0xFF);
}

static long distance(uint32_t a, uint32_t b)
{
    const long r = red(a) - red(b);
    const long g = green(a) - green(b);
    const long l = blue(a) - blue(b);
    return r * r + g * g + l * l;
}

/* The 256-colour palette's fixed part: the 6x6x6 cube (16-231) and the
   grey ramp (232-255), whose values terminals agree on. Its first sixteen
   are the named colours, which schemes change, so they are never chosen. */
static int nearest_256(uint32_t c)
{
    static const int level[6] = {0, 95, 135, 175, 215, 255};
    int best = 16;
    long best_distance = -1;
    for (int i = 16; i < 256; ++i)
    {
        uint32_t v;
        if (i < 232)
        {
            const int n = i - 16;
            v = (uint32_t)(level[n / 36] << 16 | level[n / 6 % 6] << 8 | level[n % 6]);
        }
        else
        {
            const int g = 8 + 10 * (i - 232);
            v = (uint32_t)(g << 16 | g << 8 | g);
        }
        const long d = distance(c, v);
        if (best_distance < 0 || d < best_distance)
        {
            best = i;
            best_distance = d;
        }
    }
    return best;
}

/* The named colour nearest by its VGA value: the last resort, as the
   terminal's scheme decides what it really shows. */
static int nearest_16(uint32_t c)
{
    static const uint32_t vga[16] = {0x000000, 0xAA0000, 0x00AA00, 0xAA5500, 0x0000AA, 0xAA00AA,
                                     0x00AAAA, 0xAAAAAA, 0x555555, 0xFF5555, 0x55FF55, 0xFFFF55,
                                     0x5555FF, 0xFF55FF, 0x55FFFF, 0xFFFFFF};
    int best = 0;
    for (int i = 1; i < 16; ++i)
    {
        if (distance(c, vga[i]) < distance(c, vga[best]))
        {
            best = i;
        }
    }
    return best;
}

/* One colour's part of the sequence: base 38 for the foreground, 48 for
   the background. */
static int colour_part(char *buffer, size_t size, int base, uint32_t c, int bits)
{
    switch (bits)
    {
        case 256:
            return snprintf(buffer, size, ";%d;5;%d", base, nearest_256(c));
        case 16:
        {
            const int n = nearest_16(c);
            return snprintf(buffer, size, ";%d", (n < 8 ? base - 8 : base + 52) + n % 8);
        }
        default:
            return snprintf(buffer, size, ";%d;2;%d;%d;%d", base, red(c), green(c), blue(c));
    }
}

int tf_colour_sgr_at(uint32_t fg, uint32_t bg, int bits, char *buffer, size_t size)
{
    int n = snprintf(buffer, size, "\x1b[0");
    n += colour_part(buffer + n, size - (size_t)n, 38, fg & 0xFFFFFFu, bits);
    n += colour_part(buffer + n, size - (size_t)n, 48, bg & 0xFFFFFFu, bits);
    n += snprintf(buffer + n, size - (size_t)n, "%sm", (fg & TF_UNDERLINE) != 0 ? ";4" : "");
    return n;
}

int tf_colour_sgr(uint32_t fg, uint32_t bg, char *buffer, size_t size)
{
    return tf_colour_sgr_at(fg, bg, depth(), buffer, size);
}
