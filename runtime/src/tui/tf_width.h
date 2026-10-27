/* SPDX-License-Identifier: 0BSD */
/*
 * tf_width.h -- the tables tf_width_tables.c holds (generated) and
 * tf_char_width (tf_tui.h) searches.
 */
#ifndef TERMFORMS_TF_WIDTH_H
#define TERMFORMS_TF_WIDTH_H

#include <stdint.h>

typedef struct tf_width_range
{
    uint32_t first;
    uint32_t last;
} tf_width_range;

extern const tf_width_range tf_zero_width[];
extern const int tf_zero_width_count;
extern const tf_width_range tf_wide[];
extern const int tf_wide_count;

#endif /* TERMFORMS_TF_WIDTH_H */
