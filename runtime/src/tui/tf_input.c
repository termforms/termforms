/* SPDX-License-Identifier: 0BSD */
/*
 * tf_input.c -- the source of key events: the terminal, or a script.
 *
 * Headless key scripts (TERMFORMS_HEADLESS) are plain text: every character
 * is typed as itself, except brace-enclosed names for special keys:
 *
 *   {ENTER} {TAB} {BACKTAB} {BKSP} {DEL} {LEFT} {RIGHT} {UP} {DOWN}
 *   {HOME} {END} {ESC} {F1}..{F12} {CTRL+<letter>} {ALT+<letter>}, CTRL+ /
 *   ALT+ / SHIFT+ before F<n> or a name ({CTRL+F4}, {SHIFT+LEFT}), {WAIT n}
 *   (n ms pass, so Timers run), the mouse at cell x,y (from 0, 0 at the
 *   top left): {CLICK x,y} {DBLCLICK x,y} {PRESS x,y} {DRAG x,y}
 *   {RELEASE x,y} {WHEELUP x,y} {WHEELDOWN x,y}, and {{ (a literal brace)
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

/* The rest of a script name that is several events ({CLICK}: press, then
   release), handed out one per call. */
static tf_key queued[4];
static int queued_count = 0;

void tf_input_start(const char *headless_script)
{
    script = headless_script;
    queued_count = 0;
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

typedef struct mouse_name
{
    const char *name;
    int actions[4]; /* TF_MOUSE_ actions in order, 0 ending them */
} mouse_name;

static const mouse_name MOUSE_NAMES[] = {
    {"CLICK", {TF_MOUSE_PRESS, TF_MOUSE_RELEASE, 0, 0}},
    {"DBLCLICK", {TF_MOUSE_PRESS, TF_MOUSE_RELEASE, TF_MOUSE_PRESS, TF_MOUSE_RELEASE}},
    {"PRESS", {TF_MOUSE_PRESS, 0, 0, 0}},
    {"DRAG", {TF_MOUSE_DRAG, 0, 0, 0}},
    {"RELEASE", {TF_MOUSE_RELEASE, 0, 0, 0}},
    {"WHEELUP", {TF_MOUSE_WHEEL_UP, 0, 0, 0}},
    {"WHEELDOWN", {TF_MOUSE_WHEEL_DOWN, 0, 0, 0}},
};

/* "{CLICK 10,3}": queues its events and returns the first; 0 if the name
   is not a mouse one. */
static int scripted_mouse(const char *name, size_t length, int mods, tf_key *key)
{
    for (size_t i = 0; i < sizeof MOUSE_NAMES / sizeof MOUSE_NAMES[0]; ++i)
    {
        const size_t n = strlen(MOUSE_NAMES[i].name);
        int x = 0;
        int y = 0;
        if (length <= n || strncmp(name, MOUSE_NAMES[i].name, n) != 0 || name[n] != ' ' ||
            sscanf(name + n + 1, "%d,%d", &x, &y) != 2)
        {
            continue;
        }
        queued_count = 0;
        for (int k = 3; k >= 1; --k) /* queued in reverse: taken from the end */
        {
            if (MOUSE_NAMES[i].actions[k] != 0)
            {
                queued[queued_count++] =
                    (tf_key){TF_KEY_MOUSE, (uint32_t)MOUSE_NAMES[i].actions[k], mods, x, y};
            }
        }
        *key = (tf_key){TF_KEY_MOUSE, (uint32_t)MOUSE_NAMES[i].actions[0], mods, x, y};
        return 1;
    }
    return 0;
}

static tf_key next_scripted(void)
{
    tf_key key = {TF_KEY_EOF, 0, 0, 0, 0};
    if (queued_count > 0)
    {
        return queued[--queued_count];
    }

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
        if (length > 5 && strncmp(name, "WAIT ", 5) == 0)
        {
            key.kind = TF_KEY_WAIT; /* {WAIT 1500}: time passes, for Timers */
            key.ch = (uint32_t)atoi(name + 5);
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
        if (scripted_mouse(name, length, mods, &key))
        {
            return key;
        }
        for (size_t i = 0; i < sizeof NAMED_KEYS / sizeof NAMED_KEYS[0]; ++i)
        {
            if (strlen(NAMED_KEYS[i].name) == length &&
                strncmp(NAMED_KEYS[i].name, name, length) == 0)
            {
                key.kind = NAMED_KEYS[i].kind;
                key.mods = mods; /* {SHIFT+LEFT}, {CTRL+RIGHT} */
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
    return tf_input_wait(-1);
}

tf_key tf_input_wait(int timeout_ms)
{
    if (script != NULL)
    {
        return next_scripted();
    }

    tf_key key = {TF_KEY_NONE, 0, 0, 0, 0};
    const int64_t deadline = timeout_ms >= 0 ? tf_platform_monotonic_ms() + timeout_ms : -1;
    for (;;)
    {
        if (tf_keys_next(&decoder, &key, 0))
        {
            return key;
        }

        /* A partial escape sequence gets a short grace period; otherwise
         * wait for the user, or until the deadline (a Timer is due). */
        const int partial = decoder.count > 0;
        int wait = partial ? TF_ESC_TIMEOUT_MS : -1;
        if (deadline >= 0)
        {
            const int64_t left = deadline - tf_platform_monotonic_ms();
            if (left <= 0 && !partial)
            {
                key.kind = TF_KEY_NONE;
                return key;
            }
            wait = wait < 0 || left < wait ? (int)(left > 0 ? left : 0) : wait;
        }
        /* Read only what the decoder has room for: the rest of a long paste
           waits in the terminal's buffer instead of being lost. */
        unsigned char bytes[sizeof decoder.pending];
        const int room = (int)sizeof decoder.pending - decoder.count;
        if (room == 0)
        {
            tf_keys_next(&decoder, &key, 1); /* full of something undecodable */
            continue;
        }
        const int n = tf_term_read(bytes, room, wait);

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
