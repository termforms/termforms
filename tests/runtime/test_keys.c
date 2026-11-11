/* SPDX-License-Identifier: 0BSD */
/*
 * test_keys.c -- the key decoder on the bytes a terminal sends: modifiers
 * on arrows and function keys, SGR mouse reports (press, drag, release,
 * the wheel; other buttons ignored), bracketed paste (text only, line
 * breaks and tabs as the characters LF and TAB), sequences split across
 * reads, and the base64 of copied text sent to the terminal (OSC 52).
 * Key scripts never reach this code, so the run tests cannot cover it.
 */
#include "../../runtime/src/tui/tf_tui.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

/* Feeds bytes and checks the one key they make. */
static void expect_key(const char *bytes, tf_key_kind kind, uint32_t ch, int mods, int x, int y)
{
    tf_keys keys;
    memset(&keys, 0, sizeof keys);
    tf_keys_feed(&keys, (const unsigned char *)bytes, (int)strlen(bytes));
    tf_key key = {TF_KEY_NONE, 0, 0, 0, 0, 0};
    const int got = tf_keys_next(&keys, &key, 0);
    if (!got || key.kind != kind || key.ch != ch || key.mods != mods ||
        (kind == TF_KEY_MOUSE && (key.x != x || key.y != y)) || keys.count != 0)
    {
        fprintf(stderr, "FAIL: \"\\e%s\": got kind %d ch %u mods %d at %d,%d (%d left)\n",
                bytes + 1, (int)key.kind, (unsigned)key.ch, key.mods, key.x, key.y, keys.count);
        ++failures;
    }
}

/* Feeds a mouse report and checks its action and button. */
static void expect_button(const char *bytes, uint32_t ch, int button)
{
    tf_keys keys;
    memset(&keys, 0, sizeof keys);
    tf_keys_feed(&keys, (const unsigned char *)bytes, (int)strlen(bytes));
    tf_key key = {TF_KEY_NONE, 0, 0, 0, 0, 0};
    if (!tf_keys_next(&keys, &key, 0) || key.kind != TF_KEY_MOUSE || key.ch != ch ||
        key.button != button)
    {
        fprintf(stderr, "FAIL: \"\\e%s\": got kind %d ch %u button %d\n", bytes + 1, (int)key.kind,
                (unsigned)key.ch, key.button);
        ++failures;
    }
}

/* Feeds bytes that make no key (consumed and ignored). */
static void expect_nothing(const char *bytes)
{
    tf_keys keys;
    memset(&keys, 0, sizeof keys);
    tf_keys_feed(&keys, (const unsigned char *)bytes, (int)strlen(bytes));
    tf_key key = {TF_KEY_NONE, 0, 0, 0, 0, 0};
    if (tf_keys_next(&keys, &key, 0) || keys.count != 0)
    {
        fprintf(stderr, "FAIL: \"\\e%s\" gave a key or left bytes\n", bytes + 1);
        ++failures;
    }
}

int main(void)
{
    expect_key("\x1b[D", TF_KEY_LEFT, 0, 0, 0, 0);
    expect_key("\x1b[1;2D", TF_KEY_LEFT, 0, TF_MOD_SHIFT, 0, 0);
    expect_key("\x1b[1;5C", TF_KEY_RIGHT, 0, TF_MOD_CTRL, 0, 0);
    expect_key("\x1b[1;6H", TF_KEY_HOME, 0, TF_MOD_SHIFT | TF_MOD_CTRL, 0, 0);
    expect_key("\x1b[3;2~", TF_KEY_DELETE, 0, TF_MOD_SHIFT, 0, 0);
    expect_key("\x1b[1;5S", TF_KEY_F, 4, TF_MOD_CTRL, 0, 0);
    expect_key("\x1b[21~", TF_KEY_F, 10, 0, 0, 0);

    expect_key("\x1b[<0;10;5M", TF_KEY_MOUSE, TF_MOUSE_PRESS, 0, 9, 4);
    expect_key("\x1b[<0;10;5m", TF_KEY_MOUSE, TF_MOUSE_RELEASE, 0, 9, 4);
    expect_key("\x1b[<32;11;5M", TF_KEY_MOUSE, TF_MOUSE_DRAG, 0, 10, 4);
    expect_key("\x1b[<16;1;1M", TF_KEY_MOUSE, TF_MOUSE_PRESS, TF_MOD_CTRL, 0, 0);
    expect_key("\x1b[<64;3;4M", TF_KEY_MOUSE, TF_MOUSE_WHEEL_UP, 0, 2, 3);
    expect_key("\x1b[<65;3;4M", TF_KEY_MOUSE, TF_MOUSE_WHEEL_DOWN, 0, 2, 3);
    expect_button("\x1b[<0;3;4M", TF_MOUSE_PRESS, TF_BUTTON_LEFT);
    expect_button("\x1b[<2;3;4M", TF_MOUSE_PRESS, TF_BUTTON_RIGHT);
    expect_button("\x1b[<1;3;4m", TF_MOUSE_RELEASE, TF_BUTTON_MIDDLE);
    expect_button("\x1b[<34;3;4M", TF_MOUSE_DRAG, TF_BUTTON_RIGHT);
    expect_button("\x1b[<64;3;4M", TF_MOUSE_WHEEL_UP, 0);
    expect_nothing("\x1b[<35;3;4M"); /* a move with no button (mode 1003) */

    /* A report split across two reads waits for its end. */
    tf_keys keys;
    memset(&keys, 0, sizeof keys);
    tf_key key = {TF_KEY_NONE, 0, 0, 0, 0, 0};
    tf_keys_feed(&keys, (const unsigned char *)"\x1b[<0;12", 7);
    if (tf_keys_next(&keys, &key, 0))
    {
        fprintf(stderr, "FAIL: half a mouse report gave a key\n");
        ++failures;
    }
    tf_keys_feed(&keys, (const unsigned char *)";3M", 3);
    if (!tf_keys_next(&keys, &key, 0) || key.kind != TF_KEY_MOUSE || key.x != 11 || key.y != 2)
    {
        fprintf(stderr, "FAIL: the mouse report's second half\n");
        ++failures;
    }

    /* A paste: its line breaks (CR LF once, a lone CR too) become LF and
       its tab TAB, as characters; its other control bytes go; split in
       the middle of the end marker; then Enter after it is a key again. */
    const char *pasted = "\x1b[200~a\r\nb\tc\x01\xC3\xA9\rd\x1b[20";
    memset(&keys, 0, sizeof keys);
    tf_keys_feed(&keys, (const unsigned char *)pasted, (int)strlen(pasted));
    const uint32_t want[] = {'a', '\n', 'b', '\t', 'c', 0xE9, '\n', 'd'};
    for (size_t i = 0; i < sizeof want / sizeof want[0]; ++i)
    {
        if (!tf_keys_next(&keys, &key, 0) || key.kind != TF_KEY_CHAR || key.ch != want[i])
        {
            fprintf(stderr, "FAIL: pasted character %d: kind %d ch %u\n", (int)i, (int)key.kind,
                    (unsigned)key.ch);
            ++failures;
        }
    }
    if (tf_keys_next(&keys, &key, 0))
    {
        fprintf(stderr, "FAIL: half the paste's end marker gave a key\n");
        ++failures;
    }
    tf_keys_feed(&keys, (const unsigned char *)"1~\r", 3);
    if (!tf_keys_next(&keys, &key, 0) || key.kind != TF_KEY_ENTER || keys.pasting)
    {
        fprintf(stderr, "FAIL: Enter after the paste\n");
        ++failures;
    }

    if (failures == 0)
    {
        printf("keys: all passed\n");
    }
    /* The base64 that carries copied text to the terminal (OSC 52):
       padding for 1, 2 and 3 bytes, and UTF-8 passing through as bytes. */
    {
        static const struct
        {
            const char *in, *want;
        } kCases[] = {{"", ""},        {"M", "TQ=="},    {"Ma", "TWE="},
                      {"Man", "TWFu"}, {"a\nb", "YQpi"}, {"\xC3\xA9t\xC3\xA9", "w6l0w6k="}};
        for (size_t i = 0; i < sizeof kCases / sizeof kCases[0]; ++i)
        {
            char out[32];
            const size_t n = tf_base64(kCases[i].in, strlen(kCases[i].in), out);
            out[n] = '\0';
            if (strcmp(out, kCases[i].want) != 0)
            {
                fprintf(stderr, "FAIL: base64 of case %d: %s, not %s\n", (int)i, out,
                        kCases[i].want);
                ++failures;
            }
        }
    }

    return failures == 0 ? 0 : 1;
}
