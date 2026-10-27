/* SPDX-License-Identifier: 0BSD */
/*
 * tf_array.c -- arrays: 0-based, column-major, bounds-checked, fixed or
 * dynamic (see tf_runtime.h for the rules).
 *
 * Every element type is valid when its bytes are zero (0, 0.0, False, the
 * NULL that is ""), so new elements are made with calloc or memset.
 *
 * After tf_raise the program may carry on to its error check (inside a
 * Try), so a failed element lookup returns a scratch element instead of
 * NULL; whatever the statement then reads or writes there is harmless.
 */
#include "../tf_internal.h"

#include <stdlib.h>
#include <string.h>

static size_t elem_size(int32_t elem)
{
    switch (elem)
    {
        case TF_ELEM_I32:
            return sizeof(int32_t);
        case TF_ELEM_I64:
            return sizeof(int64_t);
        case TF_ELEM_F64:
            return sizeof(double);
        case TF_ELEM_BOOL:
            return sizeof(int);
        default:
            return sizeof(tf_string *);
    }
}

/* The element handed out after an index error, and whether it may hold a
   string the program stored into it (released when it is handed out again). */
static unsigned char scratch[sizeof(double) > sizeof(void *) ? sizeof(double) : sizeof(void *)];
static int scratch_has_string = 0;

static void *scratch_element(int32_t elem)
{
    if (scratch_has_string)
    {
        tf_string *held = NULL;
        memcpy(&held, scratch, sizeof held);
        tf_str_release(held);
    }
    memset(scratch, 0, sizeof scratch);
    scratch_has_string = elem == TF_ELEM_STRING;
    return scratch;
}

static void *out_of_range(const tf_array *a)
{
    tf_raise(TF_ERR_SUBSCRIPT, NULL);
    return scratch_element(a->elem);
}

/* Elements in the shape upper[0..rank), or -1 if a bound is below -1. Too
   many to address at all is out of memory. */
static int64_t element_count(int32_t rank, const int32_t *upper, size_t size)
{
    int64_t count = 1;
    for (int32_t d = 0; d < rank; ++d)
    {
        if (upper[d] < -1)
        {
            return -1;
        }
        const int64_t extent = (int64_t)upper[d] + 1;
        if (extent != 0 && count > (INT64_MAX / (int64_t)size) / extent)
        {
            tf_fatal_error(TF_ERR_OUT_OF_MEMORY, NULL);
        }
        count *= extent;
    }
    return count;
}

static void release_strings(tf_array *a, int64_t from, int64_t to)
{
    if (a->elem != TF_ELEM_STRING || a->data == NULL)
    {
        return;
    }
    tf_string **strings = a->data;
    for (int64_t i = from; i < to; ++i)
    {
        tf_str_release(strings[i]);
        strings[i] = NULL;
    }
}

static void set_shape(tf_array *a, int32_t elem, int32_t rank, const int32_t *upper, int64_t count)
{
    a->elem = elem;
    a->rank = rank;
    a->count = count;
    for (int32_t d = 0; d < TF_ARRAY_MAX_RANK; ++d)
    {
        a->upper[d] = d < rank ? upper[d] : 0;
    }
}

void tf_array_init(tf_array *a, tf_elem elem, int32_t rank, const int32_t *upper)
{
    const size_t size = elem_size((int32_t)elem);
    const int64_t count = element_count(rank, upper, size);
    memset(a, 0, sizeof *a);
    set_shape(a, (int32_t)elem, rank, upper, count < 0 ? 0 : count);
    a->fixed = 1;
    if (a->count > 0)
    {
        a->data = tf_xcalloc((size_t)a->count, size);
    }
}

void tf_array_redim(tf_array *a, tf_elem elem, int preserve, int32_t rank, const int32_t *upper)
{
    if (a->locks > 0 || a->fixed)
    {
        tf_raise(TF_ERR_ARRAY_LOCKED, NULL);
        return;
    }
    const size_t size = elem_size((int32_t)elem);
    const int64_t count = element_count(rank, upper, size);
    if (count < 0)
    {
        tf_raise(TF_ERR_SUBSCRIPT, NULL);
        return;
    }

    if (!preserve || a->rank == 0)
    {
        tf_array_free(a);
        set_shape(a, (int32_t)elem, rank, upper, count);
        if (count > 0)
        {
            a->data = tf_xcalloc((size_t)count, size);
        }
        return;
    }

    /* Preserve: as VB, only the last dimension may change; column-major
       order keeps the existing elements at the front. */
    for (int32_t d = 0; d + 1 < rank; ++d)
    {
        if (rank != a->rank || upper[d] != a->upper[d])
        {
            tf_raise(TF_ERR_SUBSCRIPT, NULL);
            return;
        }
    }
    if (count < a->count)
    {
        release_strings(a, count, a->count);
    }
    if (count == 0)
    {
        free(a->data);
        a->data = NULL;
    }
    else
    {
        a->data = tf_xrealloc(a->data, (size_t)count * size);
        if (count > a->count)
        {
            memset((char *)a->data + (size_t)a->count * size, 0, (size_t)(count - a->count) * size);
        }
    }
    set_shape(a, (int32_t)elem, rank, upper, count);
}

void tf_array_erase(tf_array *a)
{
    if (a->locks > 0)
    {
        tf_raise(TF_ERR_ARRAY_LOCKED, NULL);
        return;
    }
    if (a->fixed)
    {
        release_strings(a, 0, a->count);
        if (a->data != NULL)
        {
            memset(a->data, 0, (size_t)a->count * elem_size(a->elem));
        }
        return;
    }
    tf_array_free(a);
}

void tf_array_free(tf_array *a)
{
    release_strings(a, 0, a->count);
    free(a->data);
    const int32_t elem = a->elem;
    const int32_t fixed = a->fixed;
    memset(a, 0, sizeof *a);
    a->elem = elem;
    a->fixed = fixed;
}

void tf_array_copy(tf_array *to, const tf_array *from)
{
    if (to == from)
    {
        return;
    }
    if (to->locks > 0 || to->fixed)
    {
        tf_raise(TF_ERR_ARRAY_LOCKED, NULL);
        return;
    }
    tf_array_free(to);
    set_shape(to, from->elem, from->rank, from->upper, from->count);
    if (from->count > 0)
    {
        const size_t bytes = (size_t)from->count * elem_size(from->elem);
        to->data = tf_xmalloc(bytes);
        memcpy(to->data, from->data, bytes);
        if (from->elem == TF_ELEM_STRING)
        {
            tf_string **strings = to->data;
            for (int64_t i = 0; i < to->count; ++i)
            {
                tf_str_retain(strings[i]);
            }
        }
    }
}

void *tf_array_at1(tf_array *a, int64_t i)
{
    if (a->rank != 1 || i < 0 || i > a->upper[0])
    {
        return out_of_range(a);
    }
    return (char *)a->data + (size_t)i * elem_size(a->elem);
}

void *tf_array_at2(tf_array *a, int64_t i, int64_t j)
{
    if (a->rank != 2 || i < 0 || i > a->upper[0] || j < 0 || j > a->upper[1])
    {
        return out_of_range(a);
    }
    const int64_t linear = i + ((int64_t)a->upper[0] + 1) * j;
    return (char *)a->data + (size_t)linear * elem_size(a->elem);
}

void *tf_array_atn(tf_array *a, int32_t rank, const int64_t *index)
{
    if (a->rank != rank)
    {
        return out_of_range(a);
    }
    int64_t linear = 0;
    int64_t stride = 1;
    for (int32_t d = 0; d < rank; ++d)
    {
        if (index[d] < 0 || index[d] > a->upper[d])
        {
            return out_of_range(a);
        }
        linear += index[d] * stride;
        stride *= (int64_t)a->upper[d] + 1;
    }
    return (char *)a->data + (size_t)linear * elem_size(a->elem);
}

int32_t tf_array_lbound(const tf_array *a, int32_t dimension)
{
    if (a->rank == 0 || dimension < 1 || dimension > a->rank)
    {
        tf_raise(TF_ERR_SUBSCRIPT, NULL);
    }
    return 0;
}

int32_t tf_array_ubound(const tf_array *a, int32_t dimension)
{
    if (a->rank == 0 || dimension < 1 || dimension > a->rank)
    {
        tf_raise(TF_ERR_SUBSCRIPT, NULL);
        return 0;
    }
    return a->upper[dimension - 1];
}

void tf_array_lock(tf_array *a)
{
    ++a->locks;
}

void tf_array_unlock(tf_array *a)
{
    --a->locks;
}
