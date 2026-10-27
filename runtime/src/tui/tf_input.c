/* SPDX-License-Identifier: 0BSD */
/*
 * tf_input.c -- the source of key events: the terminal, or a script.
 *
 * Headless key scripts (TERMFORMS_HEADLESS) are plain text: every character
 * is typed as itself, except brace-enclosed names for special keys:
 *
 *   {ENTER} {TAB} {BACKTAB} {BKSP} {DEL} {LEFT} {RIGHT} {UP} {DOWN}
 *   {HOME} {END} {ESC} {F1}..{F12} {CTRL+<letter>} {ALT+<letter>}, CTRL+ /
 *   ALT+ / SHIFT+ before F<n> ({CTRL+F4}), and {{ (a literal brace)
 *
 * When the script runs out, the input reports TF_KEY_EOF and the form
 * closes, so a test can never hang waiting for a key.
 */
#include "tf_tui.h"

#include <stdlib.h>
#include <string.h>

/* Milliseconds to wait for the rest of an escape sequence before deciding
 * the user pressed Esc on its own. Long enough for SSH, short enough to feel
 * instant. */
#define TF_ESC_TIMEOUT_MS 50

static const char *script = NULL;
static tf_keys decoder;

void tf_input_start(const char *headless_script)
{
    script = headless_script;
    memset(&decoder, 0, sizeof decoder);
}

typedef struct named_key
{
    const char *name;
    tf_key_kind kind;
} named_key;

static const named_key NAMED_KEYS[] = {
    {"ENTER", TF_KEY_ENTER},    {"TAB", TF_KEY_TAB},    {"BACKTAB", TF_KEY_BACKTAB},
    {"BKSP", TF_KEY_BACKSPACE}, {"DEL", TF_KEY_DELETE}, {"LEFT", TF_KEY_LEFT},
    {"RIGHT", TF_KEY_RIGHT},    {"UP", TF_KEY_UP},      {"DOWN", TF_KEY_DOWN},
    {"HOME", TF_KEY_HOME},      {"END", TF_KEY_END},    {"ESC", TF_KEY_ESC},
    {"PGUP", TF_KEY_PGUP},      {"PGDN", TF_KEY_PGDN},
};

static tf_key next_scripted(void)
{
    tf_key key = {TF_KEY_EOF, 0, 0};

    if (*script == '\0')
    {
        return key;
    }

    if (script[0] == '{' && script[1] == '{')
    {
        script += 2;
        key.kind = TF_KEY_CHAR;
        key.ch = '{';
        return key;
    }

    if (script[0] == '{')
    {
        const char *close = strchr(script, '}');
        if (close == NULL)
        {
            tf_fatal_error(TF_ERR_INVALID_ARGUMENT, "Unterminated {key} in TERMFORMS_HEADLESS");
        }
        size_t length = (size_t)(close - script - 1);
        const char *name = script + 1;
        script = close + 1;

        /* CTRL+, ALT+ and SHIFT+ prefixes, then a letter, F1-F12 or a name. */
        int mods = 0;
        for (;;)
        {
            if (length > 5 && strncmp(name, "CTRL+", 5) == 0)
            {
                mods |= TF_MOD_CTRL;
                name += 5;
                length -= 5;
            }
            else if (length > 4 && strncmp(name, "ALT+", 4) == 0)
            {
                mods |= TF_MOD_ALT;
                name += 4;
                length -= 4;
            }
            else if (length > 6 && strncmp(name, "SHIFT+", 6) == 0)
            {
                mods |= TF_MOD_SHIFT;
                name += 6;
                length -= 6;
            }
            else
            {
                break;
            }
        }
        if (length == 1 && mods != 0)
        {
            const char c = name[0];
            key.kind = mods == TF_MOD_CTRL ? TF_KEY_CTRL : TF_KEY_ALT;
            key.ch = (uint32_t)(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
            return key;
        }
        if (length >= 2 && length <= 3 && name[0] == 'F' && name[1] >= '1' && name[1] <= '9')
        {
            const int n = atoi(name + 1);
            if (n >= 1 && n <= 12)
            {
                key.kind = TF_KEY_F;
                key.ch = (uint32_t)n;
                key.mods = mods;
                return key;
            }
        }
        for (size_t i = 0; i < sizeof NAMED_KEYS / sizeof NAMED_KEYS[0]; ++i)
        {
            if (strlen(NAMED_KEYS[i].name) == length &&
                strncmp(NAMED_KEYS[i].name, name, length) == 0)
            {
                key.kind = NAMED_KEYS[i].kind;
                return key;
            }
        }
        tf_fatal_error(TF_ERR_INVALID_ARGUMENT, "Unknown {key} in TERMFORMS_HEADLESS");
    }

    uint32_t cp = 0;
    script += tf_utf8_decode(script, (int)strlen(script), &cp);
    key.kind = TF_KEY_CHAR;
    key.ch = cp;
    return key;
}

tf_key tf_input_next(void)
{
    if (script != NULL)
    {
        return next_scripted();
    }

    tf_key key = {TF_KEY_NONE, 0, 0};
    for (;;)
    {
        if (tf_keys_next(&decoder, &key, 0))
        {
            return key;
        }

        /* A partial escape sequence gets a short grace period; otherwise
         * wait indefinitely for the user. */
        const int partial = decoder.count > 0;
        unsigned char bytes[64];
        const int n = tf_term_read(bytes, (int)sizeof bytes, partial ? TF_ESC_TIMEOUT_MS : -1);

        if (tf_term_take_resize())
        {
            key.kind = TF_KEY_RESIZE;
            return key;
        }
        if (n > 0)
        {
            tf_keys_feed(&decoder, bytes, n);
        }
        else if (partial && tf_keys_next(&decoder, &key, 1))
        {
            return key; /* the quiet period expired: a lone Esc */
        }
    }
}
