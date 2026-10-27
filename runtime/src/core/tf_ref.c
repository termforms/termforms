/* SPDX-License-Identifier: 0BSD */
/*
 * tf_ref.c -- Refs: reference-counted records on the heap (see
 * tf_runtime.h). A record sits behind a header holding its count and its
 * Type's tf_record_ops, so the last release can free what it holds without
 * the caller knowing its Type.
 *
 * Releasing a record releases the Refs it holds, recursively; a very long
 * chain (a linked list of millions) is freed with as many nested calls.
 */
#include "../tf_internal.h"

#include <stdlib.h>

/* Room for the header, rounded so the record after it is aligned for any
   member type. */
typedef struct ref_header
{
    int64_t count;
    const tf_record_ops *ops;
} ref_header;

#define TF_REF_HEADER_SIZE ((sizeof(ref_header) + 15u) / 16u * 16u)

static ref_header *header_of(void *record)
{
    return (ref_header *)(void *)((char *)record - TF_REF_HEADER_SIZE);
}

void *tf_ref_new(const tf_record_ops *ops)
{
    char *block = tf_xmalloc(TF_REF_HEADER_SIZE + ops->size);
    ref_header *h = (ref_header *)(void *)block;
    h->count = 1;
    h->ops = ops;
    void *record = block + TF_REF_HEADER_SIZE;
    ops->init(record);
    return record;
}

void *tf_ref_new_copy(const tf_record_ops *ops, const void *from)
{
    void *record = tf_ref_new(ops);
    ops->copy(record, from);
    return record;
}

void tf_ref_retain(void *record)
{
    if (record != NULL)
    {
        ++header_of(record)->count;
    }
}

void tf_ref_release(void *record)
{
    if (record == NULL)
    {
        return;
    }
    ref_header *h = header_of(record);
    if (--h->count == 0)
    {
        h->ops->free(record);
        free(h);
    }
}

void *tf_ref_ref(void *record)
{
    tf_ref_retain(record);
    return record;
}

void *tf_ref_replace(void *old, void *owned)
{
    tf_ref_release(old);
    return owned;
}

void *tf_ref_get(void *record, const tf_record_ops *ops)
{
    if (record == NULL)
    {
        tf_raise(TF_ERR_NOTHING, NULL);
        return tf_scratch_element(TF_ELEM_RECORD, ops);
    }
    return record;
}
