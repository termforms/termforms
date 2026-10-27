/* SPDX-License-Identifier: 0BSD */
/*
 * tf_array.c -- arrays: 0-based, column-major, bounds-checked, fixed or
 * dynamic (see tf_runtime.h for the rules).
 *
 * A Dictionary is an array plus a key index (see "Dictionaries" below).
 *
 * Every scalar element type is valid when its bytes are zero (0, 0.0,
 * False, the NULL that is ""), so new elements are made with calloc or
 * memset; a record element is then also given its Type's init, and is
 * freed and copied through its tf_record_ops.
 *
 * After tf_raise the program may carry on to its error check (inside a
 * Try), so a failed element lookup returns a scratch element instead of
 * NULL; whatever the statement then reads or writes there is harmless.
 */
#include "../tf_internal.h"

#include <stdlib.h>
#include <string.h>

static void free_index(tf_array *a);
static void copy_index(tf_array *to, const tf_array *from);

static size_t elem_size(int32_t elem, const tf_record_ops *ops)
{
    switch (elem)
    {
        case TF_ELEM_RECORD:
            return ops->size;
        case TF_ELEM_I32:
            return sizeof(int32_t);
        case TF_ELEM_I64:
            return sizeof(int64_t);
        case TF_ELEM_F64:
            return sizeof(double);
        case TF_ELEM_BOOL:
            return sizeof(int);
        case TF_ELEM_REF:
            return sizeof(void *);
        default:
            return sizeof(tf_string *);
    }
}

/* The element handed out after an index error. The program may store into
   it (a string, a record's strings), so whatever it holds is released when
   it is handed out again. Global state, as tf_error.c's: one program,
   one thread. */
static unsigned char *scratch = NULL;
static size_t scratch_size = 0;
static int32_t scratch_elem = TF_ELEM_I32;
static const tf_record_ops *scratch_ops = NULL;

void *tf_scratch_element(int32_t elem, const tf_record_ops *ops)
{
    if (scratch_elem == TF_ELEM_STRING && scratch != NULL)
    {
        tf_string *held = NULL;
        memcpy(&held, scratch, sizeof held);
        tf_str_release(held);
    }
    else if (scratch_elem == TF_ELEM_RECORD && scratch != NULL)
    {
        scratch_ops->free(scratch);
    }
    else if (scratch_elem == TF_ELEM_REF && scratch != NULL)
    {
        void *held = NULL;
        memcpy(&held, scratch, sizeof held);
        tf_ref_release(held);
    }
    const size_t size = elem_size(elem, ops);
    if (size > scratch_size)
    {
        scratch = tf_xrealloc(scratch, size);
        scratch_size = size;
    }
    memset(scratch, 0, scratch_size);
    scratch_elem = elem;
    scratch_ops = ops;
    if (elem == TF_ELEM_RECORD)
    {
        ops->init(scratch);
    }
    return scratch;
}

static void *out_of_range(const tf_array *a)
{
    tf_raise(TF_ERR_SUBSCRIPT, NULL);
    return tf_scratch_element(a->elem, a->ops);
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

static void *element(const tf_array *a, int64_t i)
{
    return (char *)a->data + (size_t)i * elem_size(a->elem, a->ops);
}

/* Releases what elements [from, to) hold; their bytes are then zero. */
static void release_elements(tf_array *a, int64_t from, int64_t to)
{
    if (a->data == NULL)
    {
        return;
    }
    if (a->elem == TF_ELEM_STRING)
    {
        tf_string **strings = a->data;
        for (int64_t i = from; i < to; ++i)
        {
            tf_str_release(strings[i]);
            strings[i] = NULL;
        }
    }
    else if (a->elem == TF_ELEM_RECORD)
    {
        for (int64_t i = from; i < to; ++i)
        {
            a->ops->free(element(a, i));
            memset(element(a, i), 0, a->ops->size);
        }
    }
    else if (a->elem == TF_ELEM_REF)
    {
        void **refs = a->data;
        for (int64_t i = from; i < to; ++i)
        {
            tf_ref_release(refs[i]);
            refs[i] = NULL;
        }
    }
}

/* Makes elements [from, to), already zeroed, ready: records get their init. */
static void ready_elements(tf_array *a, int64_t from, int64_t to)
{
    if (a->elem == TF_ELEM_RECORD)
    {
        for (int64_t i = from; i < to; ++i)
        {
            a->ops->init(element(a, i));
        }
    }
}

static void set_shape(tf_array *a, int32_t elem, const tf_record_ops *ops, int32_t rank,
                      const int32_t *upper, int64_t count)
{
    a->elem = elem;
    a->ops = ops;
    a->rank = rank;
    a->count = count;
    for (int32_t d = 0; d < TF_ARRAY_MAX_RANK; ++d)
    {
        a->upper[d] = d < rank ? upper[d] : 0;
    }
}

void tf_array_init(tf_array *a, tf_elem elem, const tf_record_ops *ops, int32_t rank,
                   const int32_t *upper)
{
    const size_t size = elem_size((int32_t)elem, ops);
    const int64_t count = element_count(rank, upper, size);
    memset(a, 0, sizeof *a);
    set_shape(a, (int32_t)elem, ops, rank, upper, count < 0 ? 0 : count);
    a->fixed = 1;
    if (a->count > 0)
    {
        a->data = tf_xcalloc((size_t)a->count, size);
        ready_elements(a, 0, a->count);
    }
}

void tf_array_redim(tf_array *a, tf_elem elem, const tf_record_ops *ops, int preserve, int32_t rank,
                    const int32_t *upper)
{
    if (a->locks > 0 || a->fixed)
    {
        tf_raise(TF_ERR_ARRAY_LOCKED, NULL);
        return;
    }
    const size_t size = elem_size((int32_t)elem, ops);
    const int64_t count = element_count(rank, upper, size);
    if (count < 0)
    {
        tf_raise(TF_ERR_SUBSCRIPT, NULL);
        return;
    }

    if (!preserve || a->rank == 0)
    {
        tf_array_free(a);
        set_shape(a, (int32_t)elem, ops, rank, upper, count);
        if (count > 0)
        {
            a->data = tf_xcalloc((size_t)count, size);
            ready_elements(a, 0, count);
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
    const int64_t old = a->count;
    if (count < old)
    {
        release_elements(a, count, old);
    }
    if (count == 0)
    {
        free(a->data);
        a->data = NULL;
    }
    else
    {
        /* Records move with realloc: a record holds no pointers to itself. */
        a->data = tf_xrealloc(a->data, (size_t)count * size);
        if (count > old)
        {
            memset((char *)a->data + (size_t)old * size, 0, (size_t)(count - old) * size);
        }
    }
    set_shape(a, (int32_t)elem, ops, rank, upper, count);
    if (count > old)
    {
        ready_elements(a, old, count);
    }
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
        release_elements(a, 0, a->count);
        if (a->data != NULL)
        {
            memset(a->data, 0, (size_t)a->count * elem_size(a->elem, a->ops));
            ready_elements(a, 0, a->count);
        }
        return;
    }
    tf_array_free(a);
}

void tf_array_free(tf_array *a)
{
    release_elements(a, 0, a->count);
    free_index(a);
    free(a->data);
    const int32_t elem = a->elem;
    const int32_t fixed = a->fixed;
    const tf_record_ops *ops = a->ops;
    memset(a, 0, sizeof *a);
    a->elem = elem;
    a->fixed = fixed;
    a->ops = ops;
}

/* The copy itself, once the checks are done: to becomes from's shape and
   holds copies of its elements. */
static void copy_elements(tf_array *to, const tf_array *from)
{
    const int32_t fixed = to->fixed;
    tf_array_free(to);
    to->fixed = fixed;
    set_shape(to, from->elem, from->ops, from->rank, from->upper, from->count);
    if (from->count == 0)
    {
        return;
    }
    const size_t size = elem_size(from->elem, from->ops);
    if (from->elem == TF_ELEM_RECORD)
    {
        to->data = tf_xcalloc((size_t)from->count, size);
        ready_elements(to, 0, to->count);
        for (int64_t i = 0; i < to->count; ++i)
        {
            from->ops->copy(element(to, i), element(from, i));
        }
        copy_index(to, from);
        return;
    }
    to->data = tf_xmalloc((size_t)from->count * size);
    memcpy(to->data, from->data, (size_t)from->count * size);
    if (from->elem == TF_ELEM_STRING)
    {
        tf_string **strings = to->data;
        for (int64_t i = 0; i < to->count; ++i)
        {
            tf_str_retain(strings[i]);
        }
    }
    else if (from->elem == TF_ELEM_REF)
    {
        void **refs = to->data;
        for (int64_t i = 0; i < to->count; ++i)
        {
            tf_ref_retain(refs[i]);
        }
    }
    copy_index(to, from);
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
    copy_elements(to, from);
}

void tf_array_assign(tf_array *to, const tf_array *from)
{
    if (to != from)
    {
        copy_elements(to, from);
    }
}

void *tf_array_at1(tf_array *a, int64_t i)
{
    if (a->rank != 1 || i < 0 || i > a->upper[0])
    {
        return out_of_range(a);
    }
    return element(a, i);
}

void *tf_array_at2(tf_array *a, int64_t i, int64_t j)
{
    if (a->rank != 2 || i < 0 || i > a->upper[0] || j < 0 || j > a->upper[1])
    {
        return out_of_range(a);
    }
    return element(a, i + ((int64_t)a->upper[0] + 1) * j);
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
    return element(a, linear);
}

int64_t tf_array_count(const tf_array *a)
{
    return a->count;
}

void *tf_array_atlin(tf_array *a, int64_t i)
{
    if (i < 0 || i >= a->count)
    {
        return out_of_range(a);
    }
    return element(a, i);
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

/* ------------------------------------------------------------------------ */
/* Dictionaries                                                             */
/* ------------------------------------------------------------------------ */

/* The keys, parallel to the values (element i's key is keys[i]), and an
   open-addressing table from a key's hash to its entry. Removing an entry
   closes the gap and rebuilds the table: Remove is O(n), lookups and
   additions O(1) on average. */
struct tf_dict_index
{
    tf_string **keys;
    int64_t capacity;   /* entries the values and keys have room for */
    int64_t *slots;     /* an entry number, or -1 for an empty slot */
    int64_t slot_count; /* a power of two, more than twice capacity */
};

static uint64_t key_hash(const tf_string *key)
{
    /* FNV-1a: simple, and good enough for a table that is never more than
       half full. */
    uint64_t h = UINT64_C(14695981039346656037);
    const int32_t length = key != NULL ? key->length : 0;
    for (int32_t i = 0; i < length; ++i)
    {
        h ^= (unsigned char)key->data[i];
        h *= UINT64_C(1099511628211);
    }
    return h;
}

static int same_key(const tf_string *a, const tf_string *b)
{
    const int32_t la = a != NULL ? a->length : 0;
    const int32_t lb = b != NULL ? b->length : 0;
    return la == lb && (la == 0 || memcmp(a->data, b->data, (size_t)la) == 0);
}

/* The entry holding key, or -1; *slot is then where it would go. */
static int64_t find_entry(const tf_array *a, const tf_string *key, int64_t *slot)
{
    const struct tf_dict_index *x = a->dict;
    if (x == NULL)
    {
        return -1;
    }
    const uint64_t mask = (uint64_t)x->slot_count - 1;
    for (uint64_t i = key_hash(key) & mask;; i = (i + 1) & mask)
    {
        const int64_t entry = x->slots[i];
        if (entry < 0)
        {
            if (slot != NULL)
            {
                *slot = (int64_t)i;
            }
            return -1;
        }
        if (same_key(x->keys[entry], key))
        {
            return entry;
        }
    }
}

static void rebuild_slots(tf_array *a)
{
    struct tf_dict_index *x = a->dict;
    int64_t size = 8;
    while (size <= 2 * x->capacity)
    {
        size *= 2;
    }
    free(x->slots);
    x->slots = tf_xmalloc((size_t)size * sizeof *x->slots);
    x->slot_count = size;
    for (int64_t i = 0; i < size; ++i)
    {
        x->slots[i] = -1;
    }
    for (int64_t entry = 0; entry < a->count; ++entry)
    {
        int64_t slot = 0;
        find_entry(a, x->keys[entry], &slot);
        x->slots[slot] = entry;
    }
}

static void free_index(tf_array *a)
{
    struct tf_dict_index *x = a->dict;
    if (x == NULL)
    {
        return;
    }
    for (int64_t i = 0; i < a->count; ++i)
    {
        tf_str_release(x->keys[i]);
    }
    free(x->keys);
    free(x->slots);
    free(x);
    a->dict = NULL;
}

/* to's elements are already copies of from's; give it the same keys. */
static void copy_index(tf_array *to, const tf_array *from)
{
    if (from->dict == NULL)
    {
        return;
    }
    struct tf_dict_index *x = tf_xcalloc(1, sizeof *x);
    x->capacity = from->count;
    x->keys = tf_xcalloc((size_t)(from->count > 0 ? from->count : 1), sizeof *x->keys);
    for (int64_t i = 0; i < from->count; ++i)
    {
        x->keys[i] = from->dict->keys[i];
        tf_str_retain(x->keys[i]);
    }
    to->dict = x;
    rebuild_slots(to);
}

static void *add_entry(tf_array *d, tf_elem elem, const tf_record_ops *ops, tf_string *key)
{
    if (d->locks > 0)
    {
        tf_raise(TF_ERR_ARRAY_LOCKED, NULL);
        return tf_scratch_element((int32_t)elem, ops);
    }
    if (d->count >= INT32_MAX)
    {
        tf_fatal_error(TF_ERR_OUT_OF_MEMORY, NULL); /* Count is an Integer */
    }
    if (d->dict == NULL)
    {
        d->dict = tf_xcalloc(1, sizeof *d->dict);
        d->elem = (int32_t)elem;
        d->ops = ops;
        d->rank = 1;
        rebuild_slots(d);
    }
    struct tf_dict_index *x = d->dict;
    const size_t size = elem_size((int32_t)elem, ops);
    if (d->count == x->capacity)
    {
        x->capacity = x->capacity < 8 ? 8 : x->capacity * 2;
        d->data = tf_xrealloc(d->data, (size_t)x->capacity * size);
        x->keys = tf_xrealloc(x->keys, (size_t)x->capacity * sizeof *x->keys);
        rebuild_slots(d);
    }
    int64_t slot = 0;
    find_entry(d, key, &slot);
    const int64_t entry = d->count++;
    d->upper[0] = (int32_t)(d->count - 1);
    memset(element(d, entry), 0, size);
    ready_elements(d, entry, entry + 1);
    tf_str_retain(key);
    x->keys[entry] = key;
    x->slots[slot] = entry;
    return element(d, entry);
}

void *tf_dict_get(tf_array *d, tf_elem elem, const tf_record_ops *ops, const tf_string *key)
{
    const int64_t entry = find_entry(d, key, NULL);
    if (entry < 0)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
        return tf_scratch_element((int32_t)elem, ops);
    }
    return element(d, entry);
}

void *tf_dict_put(tf_array *d, tf_elem elem, const tf_record_ops *ops, tf_string *key)
{
    const int64_t entry = find_entry(d, key, NULL);
    return entry >= 0 ? element(d, entry) : add_entry(d, elem, ops, key);
}

void *tf_dict_add(tf_array *d, tf_elem elem, const tf_record_ops *ops, tf_string *key)
{
    if (find_entry(d, key, NULL) >= 0)
    {
        tf_raise(TF_ERR_KEY_EXISTS, NULL);
        return tf_scratch_element((int32_t)elem, ops);
    }
    return add_entry(d, elem, ops, key);
}

int tf_dict_exists(const tf_array *d, const tf_string *key)
{
    return find_entry(d, key, NULL) >= 0;
}

int32_t tf_dict_count(const tf_array *d)
{
    return (int32_t)d->count;
}

void tf_dict_remove(tf_array *d, const tf_string *key)
{
    if (d->locks > 0)
    {
        tf_raise(TF_ERR_ARRAY_LOCKED, NULL);
        return;
    }
    const int64_t entry = find_entry(d, key, NULL);
    if (entry < 0)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
        return;
    }
    struct tf_dict_index *x = d->dict;
    const size_t size = elem_size(d->elem, d->ops);
    release_elements(d, entry, entry + 1);
    tf_str_release(x->keys[entry]);
    const int64_t after = d->count - entry - 1;
    memmove(element(d, entry), element(d, entry + 1), (size_t)after * size);
    memmove(&x->keys[entry], &x->keys[entry + 1], (size_t)after * sizeof *x->keys);
    --d->count;
    d->upper[0] = (int32_t)(d->count - 1);
    rebuild_slots(d);
}

void tf_dict_clear(tf_array *d)
{
    if (d->locks > 0)
    {
        tf_raise(TF_ERR_ARRAY_LOCKED, NULL);
        return;
    }
    tf_array_free(d);
}

void tf_dict_keys(tf_array *to, const tf_array *d)
{
    if (to->locks > 0 || to->fixed)
    {
        tf_raise(TF_ERR_ARRAY_LOCKED, NULL);
        return;
    }
    /* Built aside first: to may be d's own member (o.Names = o.Ages.Keys). */
    tf_array keys = {0};
    const int32_t upper = (int32_t)(d->count - 1);
    set_shape(&keys, TF_ELEM_STRING, NULL, 1, &upper, d->count);
    if (d->count > 0)
    {
        tf_string **strings = tf_xmalloc((size_t)d->count * sizeof *strings);
        for (int64_t i = 0; i < d->count; ++i)
        {
            strings[i] = d->dict->keys[i];
            tf_str_retain(strings[i]);
        }
        keys.data = strings;
    }
    tf_array_free(to);
    *to = keys;
}

void tf_dict_items(tf_array *to, const tf_array *d, tf_elem elem, const tf_record_ops *ops)
{
    if (to->locks > 0 || to->fixed)
    {
        tf_raise(TF_ERR_ARRAY_LOCKED, NULL);
        return;
    }
    tf_array items = {0};
    copy_elements(&items, d);
    free_index(&items);
    const int32_t upper = (int32_t)(d->count - 1);
    set_shape(&items, (int32_t)elem, ops, 1, &upper, d->count);
    tf_array_free(to);
    *to = items;
}
