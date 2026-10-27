/* SPDX-License-Identifier: 0BSD */
/*
 * test_width.c -- character widths from the generated tables: ASCII, CJK
 * and fullwidth forms (2), emoji drawn as emoji (2), combining marks,
 * format characters and controls (0), the ambiguous ones (1), and text.
 */
#include "../../runtime/src/tui/tf_tui.h"

#include <stdio.h>

static int failures = 0;

static void expect(uint32_t cp, int want)
{
    const int got = tf_char_width(cp);
    if (got != want)
    {
        fprintf(stderr, "FAIL: U+%04X is %d cells, want %d\n", (unsigned)cp, got, want);
        ++failures;
    }
}

int main(void)
{
    expect('A', 1);
    expect(0x00E9, 1);  /* é */
    expect(0x4E2D, 2);  /* 中 */
    expect(0x3042, 2);  /* あ */
    expect(0xAC00, 2);  /* 가 */
    expect(0xFF21, 2);  /* fullwidth A */
    expect(0x1F600, 2); /* 😀 */
    expect(0x0301, 0);  /* combining acute */
    expect(0x200D, 0);  /* zero-width joiner */
    expect(0xFE0F, 0);  /* variation selector 16 */
    expect(0x1160, 0);  /* Hangul medial vowel filler */
    expect(0x0007, 0);  /* a control */
    expect(0x00AD, 1);  /* the soft hyphen is drawn */
    expect(0x00B7, 1);  /* middle dot: ambiguous, narrow */
    expect(0x2500, 1);  /* box drawing: ambiguous, narrow */
    expect(0x9FFF, 2);  /* in the ideographs block, wide even unassigned */

    const int cells = tf_text_cells("a中\xF0\x9F\x98\x80"
                                    "e\xCC\x81");
    if (cells != 6)
    {
        fprintf(stderr, "FAIL: text is %d cells, want 6\n", cells);
        ++failures;
    }
    if (failures == 0)
    {
        printf("width: all passed\n");
    }
    return failures == 0 ? 0 : 1;
}
