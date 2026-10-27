/* SPDX-License-Identifier: 0BSD */
/*
 * test_arrays.c -- the array runtime: column-major layout, ReDim Preserve
 * of the last dimension, copies that do not share, Erase of fixed and
 * dynamic arrays, string elements released (checked by the sanitizer
 * jobs), and errors 9 and 10 raised catchably.
 */
#include "termforms/tf_runtime.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define EXPECT(cond, what)                                                                         \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            fprintf(stderr, "FAIL: %s\n", what);                                                   \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

static int32_t *i32(tf_array *a, int64_t i, int64_t j)
{
    return a->rank == 1 ? tf_array_at1(a, i) : tf_array_at2(a, i, j);
}

static tf_string *text(const char *s)
{
    return tf_str_from_utf8(s, (int32_t)strlen(s));
}

int main(void)
{
    /* A fixed 2-D array is column-major: (1,0) follows (0,0). */
    tf_array grid;
    const int32_t shape[2] = {2, 3}; /* Dim grid(2, 3): 3 x 4 elements */
    tf_array_init(&grid, TF_ELEM_I32, 2, shape);
    EXPECT(grid.count == 12, "Dim grid(2, 3) has 12 elements");
    *i32(&grid, 1, 0) = 7;
    EXPECT(((int32_t *)grid.data)[1] == 7, "column-major: (1,0) is the second element");
    *i32(&grid, 2, 3) = 9;
    EXPECT(((int32_t *)grid.data)[11] == 9, "(2,3) is the last element");
    EXPECT(tf_array_ubound(&grid, 1) == 2 && tf_array_ubound(&grid, 2) == 3, "UBound");
    EXPECT(tf_array_lbound(&grid, 2) == 0, "LBound is 0");

    /* ReDim Preserve of the last dimension keeps every element in place. */
    tf_array dyn;
    memset(&dyn, 0, sizeof dyn);
    tf_array_redim(&dyn, TF_ELEM_I32, 0, 2, shape);
    *i32(&dyn, 2, 1) = 42;
    const int32_t wider[2] = {2, 9};
    tf_array_redim(&dyn, TF_ELEM_I32, 1, 2, wider);
    EXPECT(*i32(&dyn, 2, 1) == 42, "Preserve keeps (2,1)");
    EXPECT(*i32(&dyn, 2, 9) == 0, "new elements are 0");

    /* Strings: shrinking releases what is dropped; a copy does not share. */
    tf_array names;
    memset(&names, 0, sizeof names);
    const int32_t five[1] = {4};
    tf_array_redim(&names, TF_ELEM_STRING, 0, 1, five);
    for (int i = 0; i <= 4; ++i)
    {
        tf_str_assign((tf_string **)tf_array_at1(&names, i), text(i % 2 ? "odd" : "even"));
    }
    tf_array copy;
    memset(&copy, 0, sizeof copy);
    tf_array_copy(&copy, &names);
    tf_str_assign((tf_string **)tf_array_at1(&copy, 0), text("changed"));
    EXPECT(strcmp(tf_str_cstr(*(tf_string **)tf_array_at1(&names, 0)), "even") == 0,
           "changing a copy leaves the original alone");
    const int32_t two[1] = {1};
    tf_array_redim(&names, TF_ELEM_STRING, 1, 1, two);
    EXPECT(names.count == 2, "Preserve can shrink");
    EXPECT(strcmp(tf_str_cstr(*(tf_string **)tf_array_at1(&names, 1)), "odd") == 0,
           "shrinking keeps the front");

    /* Erase: a fixed array keeps its size, a dynamic one becomes empty. */
    tf_array_erase(&grid);
    EXPECT(grid.count == 12 && *i32(&grid, 1, 0) == 0, "Erase resets a fixed array");
    tf_array_erase(&names);
    EXPECT(names.rank == 0 && names.data == NULL, "Erase empties a dynamic array");

    /* Inside a Try, errors 9 and 10 are raised and the program goes on. */
    tf_try_enter();
    int32_t *bad = tf_array_at1(&dyn, 0); /* dyn is 2-D: wrong number of indexes */
    *bad = 1;                             /* lands in the scratch element */
    EXPECT(tf_error_pending && tf_err_number() == 9, "wrong rank is error 9");
    tf_err_catch();
    (void)tf_array_at2(&dyn, 3, 0);
    EXPECT(tf_error_pending && tf_err_number() == 9, "index past UBound is error 9");
    tf_err_catch();
    tf_array_lock(&dyn);
    tf_array_redim(&dyn, TF_ELEM_I32, 0, 2, shape);
    EXPECT(tf_error_pending && tf_err_number() == 10, "ReDim of a locked array is error 10");
    tf_err_catch();
    tf_array_unlock(&dyn);
    tf_array_redim(&grid, TF_ELEM_I32, 0, 2, shape);
    EXPECT(tf_error_pending && tf_err_number() == 10, "ReDim of a fixed array is error 10");
    tf_err_catch();
    (void)tf_array_ubound(&names, 1);
    EXPECT(tf_error_pending && tf_err_number() == 9, "UBound of an empty array is error 9");
    tf_err_catch();
    tf_try_leave();

    tf_array_free(&grid);
    tf_array_free(&dyn);
    tf_array_free(&names);
    tf_array_free(&copy);

    if (failures != 0)
    {
        return 1;
    }
    printf("PASS: arrays\n");
    return 0;
}
