/* SPDX-License-Identifier: 0BSD */
/*
 * tf_keys.c -- decodes terminal input bytes into key events.
 *
 * Terminals send printable text as UTF-8, control keys as single control
 * bytes, and everything else as escape sequences: CSI (ESC [ ...) or SS3
 * (ESC O x). Sequences this decoder does not know are consumed and ignored,
 * so an unexpected key can never be mistaken for typed text.
 */
#include "tf_tui.h"

#include <string.h>

void tf_keys_feed(tf_keys *keys, const unsigned char *bytes, int length)
{
    /* The buffer only ever holds one partial sequence plus a burst of typing;
     * if a paste overflows it, the excess is dropped rather than overrun. */
    const int space = (int)sizeof keys->pending - keys->count;
    const int n = length < space ? length : space;
    memcpy(keys->pending + keys->count, bytes, (size_t)n);
    keys->count += n;
}

static void consume(tf_keys *keys, int n)
{
    memmove(keys->pending, keys->pending + n, (size_t)(keys->count - n));
    keys->count -= n;
}

/* Maps the final byte of "ESC [ A".."ESC [ H" style sequences. */
static tf_key_kind letter_key(unsigned char c)
{
    switch (c)
    {
        case 'A':
            return TF_KEY_UP;
        case 'B':
            return TF_KEY_DOWN;
        case 'C':
            return TF_KEY_RIGHT;
        case 'D':
            return TF_KEY_LEFT;
        case 'H':
            return TF_KEY_HOME;
        case 'F':
            return TF_KEY_END;
        case 'Z':
            return TF_KEY_BACKTAB;
        default:
            return TF_KEY_NONE;
    }
}

/* F1-F4 as SS3 P-S, or CSI with a modifier ("ESC [ 1 ; 5 S"); 0 if not. */
static int pqrs_function(unsigned char c)
{
    return c >= 'P' && c <= 'S' ? c - 'P' + 1 : 0;
}

/* F1-F12 as "ESC [ n ~": 11-15, 17-21, 23-24 (the gaps are xterm's). */
static int tilde_function(int n)
{
    if (n >= 11 && n <= 15)
    {
        return n - 10;
    }
    if (n >= 17 && n <= 21)
    {
        return n - 11;
    }
    return n == 23 || n == 24 ? n - 12 : 0;
}

/* Maps "ESC [ n ~" sequences by their number. */
static tf_key_kind tilde_key(int n)
{
    switch (n)
    {
        case 1:
        case 7:
            return TF_KEY_HOME;
        case 3:
            return TF_KEY_DELETE;
        case 4:
        case 8:
            return TF_KEY_END;
        case 5:
            return TF_KEY_PGUP;
        case 6:
            return TF_KEY_PGDN;
        default:
            return TF_KEY_NONE;
    }
}

/*
 * Decodes an escape sequence at the start of the buffer. Returns the number of
 * bytes it occupies, 0 if it is incomplete, and sets key (kind TF_KEY_NONE for
 * sequences that are recognised as complete but ignored).
 */
static int decode_escape(const tf_keys *keys, tf_key *key)
{
    tf_key_kind *kind = &key->kind;
    const unsigned char *p = keys->pending;
    const int n = keys->count;

    if (n < 2)
    {
        return 0;
    }
    if (p[1] == 'O') /* SS3: ESC O x */
    {
        if (n < 3)
        {
            return 0;
        }
        if (pqrs_function(p[2]) != 0)
        {
            *kind = TF_KEY_F;
            key->ch = (uint32_t)pqrs_function(p[2]);
            return 3;
        }
        *kind = letter_key(p[2]);
        return 3;
    }
    if (p[1] != '[')
    {
        /* ESC followed by an ordinary byte: Alt+key (the terminal's Meta). */
        if (p[1] >= 0x20 && p[1] < 0x7F)
        {
            *kind = TF_KEY_ALT;
            key->ch = (uint32_t)(p[1] >= 'A' && p[1] <= 'Z' ? p[1] - 'A' + 'a' : p[1]);
            return 2;
        }
        *kind = TF_KEY_NONE;
        return 1;
    }

    /* CSI: parameter bytes 0x30-0x3F, intermediates 0x20-0x2F, final 0x40-0x7E.
       The first parameter picks the key, the second (if any) is 1 + the
       modifiers ("ESC [ 1 ; 5 S" is Ctrl+F4, "ESC [ 1 ; 2 D" Shift+Left). */
    int number = 0;
    int modifier = 0;
    int parameter = 0;
    for (int i = 2; i < n; ++i)
    {
        const unsigned char c = p[i];
        if (c >= '0' && c <= '9')
        {
            int *target = parameter == 0 ? &number : parameter == 1 ? &modifier : NULL;
            if (target != NULL && *target < 10000)
            {
                *target = *target * 10 + (c - '0');
            }
        }
        else if (c >= 0x40 && c <= 0x7E)
        {
            const int function = c == '~' ? tilde_function(number) : pqrs_function(c);
            if (function != 0)
            {
                *kind = TF_KEY_F;
                key->ch = (uint32_t)function;
                key->mods = modifier > 1 ? modifier - 1 : 0;
                return i + 1;
            }
            *kind = c == '~' ? tilde_key(number) : letter_key(c);
            key->mods = modifier > 1 ? modifier - 1 : 0; /* Shift+Left selects, Ctrl+Left: words */
            return i + 1;
        }
        else if (c >= 0x20)
        {
            ++parameter; /* ';' or another parameter/intermediate byte */
        }
        else
        {
            /* A control byte inside a sequence: malformed. Drop "ESC [" and
             * resynchronise on what follows. */
            *kind = TF_KEY_NONE;
            return 2;
        }
    }
    return 0; /* incomplete */
}

int tf_keys_next(tf_keys *keys, tf_key *key, int flush_escape)
{
    while (keys->count > 0)
    {
        const unsigned char c = keys->pending[0];
        key->ch = 0;
        key->mods = 0;

        if (c == 0x1B)
        {
            tf_key decoded = {TF_KEY_NONE, 0, 0};
            const int used = decode_escape(keys, &decoded);
            if (used == 0)
            {
                if (!flush_escape)
                {
                    return 0; /* wait for the rest of the sequence */
                }
                /* Quiet after ESC O or ESC [: that was Alt+O or Alt+[. */
                if (keys->count >= 2)
                {
                    key->kind = TF_KEY_ALT;
                    key->ch = keys->pending[1] == 'O' ? 'o' : keys->pending[1];
                    key->mods = 0;
                    consume(keys, 2);
                    return 1;
                }
                consume(keys, 1);
                key->kind = TF_KEY_ESC;
                return 1;
            }
            consume(keys, used);
            if (decoded.kind != TF_KEY_NONE)
            {
                *key = decoded;
                return 1;
            }
            continue;
        }

        if (c == '\r' || c == '\n')
        {
            consume(keys, 1);
            key->kind = TF_KEY_ENTER;
            return 1;
        }
        if (c == '\t')
        {
            consume(keys, 1);
            key->kind = TF_KEY_TAB;
            return 1;
        }
        if (c == 0x7F || c == 0x08)
        {
            consume(keys, 1);
            key->kind = TF_KEY_BACKSPACE;
            return 1;
        }
        if (c < 0x20)
        {
            consume(keys, 1);
            key->kind = TF_KEY_CTRL;
            key->ch = (uint32_t)(c + 'a' - 1);
            return 1;
        }

        /* UTF-8 text: wait until the whole code point has arrived. */
        const int needed = c < 0x80 ? 1 : c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
        if (keys->count < needed)
        {
            return 0;
        }
        uint32_t cp = 0;
        const int used = tf_utf8_decode((const char *)keys->pending, keys->count, &cp);
        consume(keys, used);
        key->kind = TF_KEY_CHAR;
        key->ch = cp;
        return 1;
    }
    return 0;
}
