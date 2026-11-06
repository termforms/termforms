/* SPDX-License-Identifier: 0BSD */
/*
 * test_colour.c -- the colours forms are drawn in: exact colours as SGR
 * 38;2/48;2, the nearest of the fixed 256 (never the first sixteen, which
 * colour schemes change) or of the sixteen named ones as fallbacks, the
 * underline of an access key, and a project's colour replacing a role's.
 */
#include "../../runtime/src/tui/tf_tui.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect(uint32_t fg, uint32_t bg, int bits, const char *want)
{
    char got[64];
    tf_colour_sgr_at(fg, bg, bits, got, sizeof got);
    if (strcmp(got, want) != 0)
    {
        fprintf(stderr, "FAIL: %06X on %06X at %d bits gave \\e%s, want \\e%s\n", (unsigned)fg,
                (unsigned)bg, bits, got + 1, want + 1);
        ++failures;
    }
}

int main(void)
{
    expect(0xFFFFFF, 0x0000AA, 24, "\x1b[0;38;2;255;255;255;48;2;0;0;170m");
    expect(0xAA0000 | TF_UNDERLINE, 0xAAAAAA, 24, "\x1b[0;38;2;170;0;0;48;2;170;170;170;4m");
    /* 256: white is the cube's 231 (not 15); the VGA blue's nearest is
       cube level 175 (19); grey 0xAAAAAA is the ramp's 248 (168). */
    expect(0xFFFFFF, 0x0000AA, 256, "\x1b[0;38;5;231;48;5;19m");
    expect(0x000000, 0xAAAAAA, 256, "\x1b[0;38;5;16;48;5;248m");
    /* 16: the named colours, bright ones from 90 and 100. */
    expect(0xFFFFFF, 0x0000AA, 16, "\x1b[0;97;44m");
    expect(0xAA0000 | TF_UNDERLINE, 0x00AAAA, 16, "\x1b[0;31;46;4m");
    expect(0x555555, 0x000000, 16, "\x1b[0;90;40m");

    if (tf_colour(TF_COLOUR_FORM_BACK) != 0x0000AA)
    {
        fprintf(stderr, "FAIL: the form's default is not VGA blue\n");
        ++failures;
    }
    tf_colour_set(TF_COLOUR_FORM_BACK, 0x003366);
    if (tf_colour(TF_COLOUR_FORM_BACK) != 0x003366)
    {
        fprintf(stderr, "FAIL: a project's colour did not replace the default\n");
        ++failures;
    }
    return failures == 0 ? 0 : 1;
}
