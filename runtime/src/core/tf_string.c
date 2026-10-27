/* SPDX-License-Identifier: 0BSD */
/*
 * tf_string.c -- immutable, reference-counted UTF-8 strings.
 *
 * Strings never change after creation, so sharing one between variables is
 * safe and assignment is a reference-count operation, not a copy. NULL stands
 * for the empty string throughout, which means variables need no
 * initialisation code.
 */
#include "../tf_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static tf_string *allocate(int32_t length)
{
    tf_string *s = tf_xmalloc(sizeof(tf_string) + (size_t)length + 1);
    s->refcount = 1;
    s->length = length;
    s->data[length] = '\0';
    return s;
}

tf_string *tf_str_alloc(int32_t length)
{
    return allocate(length);
}

tf_string *tf_str_from_utf8(const char *bytes, int32_t length)
{
    if (length <= 0)
    {
        return NULL;
    }
    tf_string *s = allocate(length);
    memcpy(s->data, bytes, (size_t)length);
    return s;
}

tf_string *tf_str_literal(tf_string **cache, const char *bytes, int32_t length)
{
    if (*cache == NULL && length > 0)
    {
        tf_string *s = allocate(length);
        memcpy(s->data, bytes, (size_t)length);
        s->refcount = TF_STR_IMMORTAL;
        *cache = s;
    }
    return *cache;
}

void tf_str_retain(tf_string *s)
{
    if (s != NULL && s->refcount != TF_STR_IMMORTAL)
    {
        s->refcount++;
    }
}

void tf_str_release(tf_string *s)
{
    if (s != NULL && s->refcount != TF_STR_IMMORTAL)
    {
        if (--s->refcount == 0)
        {
            free(s);
        }
    }
}

tf_string *tf_str_ref(tf_string *s)
{
    tf_str_retain(s);
    return s;
}

void tf_str_assign(tf_string **slot, tf_string *owned)
{
    /* Release after storing, in case the old and new values are the same. */
    tf_string *old = *slot;
    *slot = owned;
    tf_str_release(old);
}

tf_string *tf_str_concat(const tf_string *a, const tf_string *b)
{
    const int32_t la = a != NULL ? a->length : 0;
    const int32_t lb = b != NULL ? b->length : 0;

    if ((int64_t)la + (int64_t)lb > TF_STR_MAX_LENGTH)
    {
        /* Caught before anything is allocated, so a Try can carry on
           (VB6's error 14); give back "" as the harmless result. */
        tf_raise(TF_ERR_OUT_OF_STRING_SPACE, NULL);
        return NULL;
    }
    if (la + lb == 0)
    {
        return NULL;
    }

    tf_string *s = allocate(la + lb);
    if (la > 0)
    {
        memcpy(s->data, a->data, (size_t)la);
    }
    if (lb > 0)
    {
        memcpy(s->data + la, b->data, (size_t)lb);
    }
    return s;
}

tf_string *tf_str_from_i32(int32_t value)
{
    char buffer[16];
    const int n = snprintf(buffer, sizeof buffer, "%ld", (long)value);
    return tf_str_from_utf8(buffer, (int32_t)n);
}

int tf_str_compare(const tf_string *a, const tf_string *b)
{
    const int32_t la = a != NULL ? a->length : 0;
    const int32_t lb = b != NULL ? b->length : 0;
    const int32_t common = la < lb ? la : lb;

    if (common > 0)
    {
        const int c = memcmp(a->data, b->data, (size_t)common);
        if (c != 0)
        {
            return c;
        }
    }
    return (la > lb) - (la < lb);
}

int32_t tf_str_len(const tf_string *s)
{
    int32_t count = 0;
    if (s != NULL)
    {
        /* Count every byte that does not continue a multi-byte sequence. */
        for (int32_t i = 0; i < s->length; ++i)
        {
            if (((unsigned char)s->data[i] & 0xC0u) != 0x80u)
            {
                count++;
            }
        }
    }
    return count;
}

int32_t tf_str_lenb(const tf_string *s)
{
    return s != NULL ? s->length : 0;
}

const char *tf_str_cstr(const tf_string *s)
{
    return s != NULL ? s->data : "";
}
