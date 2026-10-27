/* SPDX-License-Identifier: 0BSD */
/*
 * tf_width.c -- how many terminal cells a character takes: 0, 1 or 2.
 *
 * The renderer measures text by this, not by code points, so that wide
 * characters (CJK, emoji) keep what follows them in the right columns.
 * The rules are in tools/gen_width_tables.py, which makes the tables.
 */
#include "tf_width.h"

#include "tf_tui.h"

#include <string.h>

static int in_ranges(uint32_t cp, const tf_width_range *ranges, int count)
{
    int lo = 0;
    int hi = count - 1;
    while (lo <= hi)
    {
        const int mid = lo + (hi - lo) / 2;
        if (cp < ranges[mid].first)
        {
            hi = mid - 1;
        }
        else if (cp > ranges[mid].last)
        {
            lo = mid + 1;
        }
        else
        {
            return 1;
        }
    }
    return 0;
}

int tf_char_width(uint32_t cp)
{
    if (cp >= 0x20 && cp < 0x7F)
    {
        return 1; /* ASCII text, the common case, without a search */
    }
    if (in_ranges(cp, tf_zero_width, tf_zero_width_count))
    {
        return 0;
    }
    return in_ranges(cp, tf_wide, tf_wide_count) ? 2 : 1;
}

int tf_text_cells(const char *utf8)
{
    int cells = 0;
    int length = (int)strlen(utf8);
    while (length > 0)
    {
        uint32_t cp = 0;
        const int used = tf_utf8_decode(utf8, length, &cp);
        cells += tf_char_width(cp);
        utf8 += used;
        length -= used;
    }
    return cells;
}
