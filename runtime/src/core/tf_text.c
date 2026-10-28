/* SPDX-License-Identifier: 0BSD */
/*
 * tf_text.c -- the string functions: Mid$, InStr, Replace, Split and the
 * rest (see tf_runtime.h). Strings are UTF-8 and positions count code
 * points, so a position is found by walking from the start: a code point
 * is a lead byte and the continuation bytes (10xxxxxx) after it. Strings
 * are trusted to be valid UTF-8 (they come from source text and from the
 * runtime's own input and file layers).
 */
#include "../tf_internal.h"

#include <stdlib.h>
#include <string.h>

static const char *bytes_of(const tf_string *s)
{
    return s != NULL ? s->data : "";
}

static int32_t length_of(const tf_string *s)
{
    return s != NULL ? s->length : 0;
}

static int continues(char c)
{
    return ((unsigned char)c & 0xC0u) == 0x80u;
}

/* The byte offset n code points on from byte `from`, or the end. */
static int32_t advance(const tf_string *s, int32_t from, int64_t n)
{
    const char *b = bytes_of(s);
    const int32_t length = length_of(s);
    int32_t i = from;
    for (; i < length && n > 0; --n)
    {
        ++i;
        while (i < length && continues(b[i]))
        {
            ++i;
        }
    }
    return i;
}

/* How many code points the first `bytes` bytes hold. */
static int32_t points_in(const char *b, int32_t bytes)
{
    int32_t count = 0;
    for (int32_t i = 0; i < bytes; ++i)
    {
        count += continues(b[i]) ? 0 : 1;
    }
    return count;
}

static tf_string *slice(const tf_string *s, int32_t from, int32_t to)
{
    return tf_str_from_utf8(bytes_of(s) + from, to - from);
}

/* Error 5; "" is the harmless result. */
static tf_string *bad_argument(void)
{
    tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
    return NULL;
}

/* A new string of length bytes, or NULL ("") for 0; error 14 if too long. */
static tf_string *make(int64_t length)
{
    if (length > TF_STR_MAX_LENGTH)
    {
        tf_raise(TF_ERR_OUT_OF_STRING_SPACE, NULL);
        return NULL;
    }
    return length > 0 ? tf_str_alloc((int32_t)length) : NULL;
}

tf_string *tf_str_mid(const tf_string *s, int32_t start, int32_t length)
{
    if (start < 1 || length < 0)
    {
        return bad_argument();
    }
    const int32_t from = advance(s, 0, start - 1);
    return slice(s, from, advance(s, from, length));
}

void tf_str_mid_assign(tf_string **slot, int32_t start, const tf_string *text, int32_t length)
{
    const tf_string *s = *slot;
    const int32_t points = tf_str_len(s);
    if (start < 1 || start > points || length < 0)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
        return;
    }
    /* As many code points as the text has, the length allows and the
       string has room for from start: the string never grows or shrinks. */
    int32_t count = tf_str_len(text);
    count = length < count ? length : count;
    count = points - (start - 1) < count ? points - (start - 1) : count;
    const int32_t from = advance(s, 0, start - 1);
    const int32_t to = advance(s, from, count);
    const int32_t taken = advance(text, 0, count);
    const int64_t size = (int64_t)length_of(s) - (to - from) + taken;
    if (size > TF_STR_MAX_LENGTH)
    {
        tf_raise(TF_ERR_OUT_OF_STRING_SPACE, NULL); /* a wider character, at the limit */
        return;
    }
    char *buffer = tf_xmalloc((size_t)size + 1);
    memcpy(buffer, bytes_of(s), (size_t)from);
    memcpy(buffer + from, bytes_of(text), (size_t)taken);
    memcpy(buffer + from + taken, bytes_of(s) + to, (size_t)(length_of(s) - to));
    tf_str_assign(slot, tf_str_from_utf8(buffer, (int32_t)size));
    free(buffer);
}

tf_string *tf_str_left(const tf_string *s, int32_t n)
{
    return n < 0 ? bad_argument() : slice(s, 0, advance(s, 0, n));
}

tf_string *tf_str_right(const tf_string *s, int32_t n)
{
    if (n < 0)
    {
        return bad_argument();
    }
    const int32_t points = tf_str_len(s);
    return slice(s, n >= points ? 0 : advance(s, 0, points - n), length_of(s));
}

/* The byte offset of the first match of find at or after byte from, whose
   end is at or before byte limit; -1 if none. last: the last such match. */
static int32_t search(const tf_string *s, const tf_string *find, int32_t from, int32_t limit,
                      int last)
{
    const char *b = bytes_of(s);
    const char *f = bytes_of(find);
    const int32_t flen = length_of(find);
    int32_t found = -1;
    for (int32_t i = from; i + flen <= limit; ++i)
    {
        /* find begins with a lead byte, so a match is never mid-character. */
        if (memcmp(b + i, f, (size_t)flen) == 0)
        {
            found = i;
            if (!last)
            {
                break;
            }
        }
    }
    return found;
}

int32_t tf_str_instr(int32_t start, const tf_string *s, const tf_string *find)
{
    if (start < 1)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
        return 0;
    }
    if (start > tf_str_len(s))
    {
        return 0;
    }
    if (length_of(find) == 0)
    {
        return start;
    }
    const int32_t at = search(s, find, advance(s, 0, start - 1), length_of(s), 0);
    return at < 0 ? 0 : points_in(bytes_of(s), at) + 1;
}

int32_t tf_str_instrrev(const tf_string *s, const tf_string *find, int32_t start)
{
    const int32_t points = tf_str_len(s);
    if (start == -1)
    {
        start = points;
    }
    else if (start < 1)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
        return 0;
    }
    if (start > points)
    {
        return 0;
    }
    if (length_of(find) == 0)
    {
        return start;
    }
    /* As VB: the match must end at or before position start. */
    const int32_t at = search(s, find, 0, advance(s, 0, start), 1);
    return at < 0 ? 0 : points_in(bytes_of(s), at) + 1;
}

static tf_string *change_case(const tf_string *s, char from, char to)
{
    tf_string *out = slice(s, 0, length_of(s));
    for (int32_t i = 0; i < length_of(out); ++i)
    {
        if (out->data[i] >= from && out->data[i] <= from + 25)
        {
            out->data[i] = (char)(out->data[i] - from + to);
        }
    }
    return out;
}

tf_string *tf_str_lcase(const tf_string *s)
{
    return change_case(s, 'A', 'a');
}

tf_string *tf_str_ucase(const tf_string *s)
{
    return change_case(s, 'a', 'A');
}

static tf_string *trim(const tf_string *s, int left, int right)
{
    const char *b = bytes_of(s);
    int32_t from = 0;
    int32_t to = length_of(s);
    while (left && from < to && b[from] == ' ')
    {
        ++from;
    }
    while (right && to > from && b[to - 1] == ' ')
    {
        --to;
    }
    return slice(s, from, to);
}

tf_string *tf_str_ltrim(const tf_string *s)
{
    return trim(s, 1, 0);
}

tf_string *tf_str_rtrim(const tf_string *s)
{
    return trim(s, 0, 1);
}

tf_string *tf_str_trim(const tf_string *s)
{
    return trim(s, 1, 1);
}

tf_string *tf_str_space(int32_t n)
{
    if (n < 0)
    {
        return bad_argument();
    }
    tf_string *out = make(n);
    if (out != NULL)
    {
        memset(out->data, ' ', (size_t)n);
    }
    return out;
}

tf_string *tf_str_repeat(int32_t n, const tf_string *s)
{
    if (n < 0 || length_of(s) == 0)
    {
        return bad_argument();
    }
    const int32_t width = advance(s, 0, 1); /* the first character's bytes */
    tf_string *out = make((int64_t)n * width);
    for (int32_t i = 0; out != NULL && i < n; ++i)
    {
        memcpy(out->data + (size_t)i * (size_t)width, bytes_of(s), (size_t)width);
    }
    return out;
}

tf_string *tf_str_chr(int32_t code)
{
    if (code < 0 || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
    {
        return bad_argument(); /* not a Unicode scalar value */
    }
    char b[4];
    int32_t n = 0;
    const uint32_t c = (uint32_t)code;
    if (c < 0x80)
    {
        b[n++] = (char)c;
    }
    else if (c < 0x800)
    {
        b[n++] = (char)(0xC0 | (c >> 6));
        b[n++] = (char)(0x80 | (c & 0x3F));
    }
    else if (c < 0x10000)
    {
        b[n++] = (char)(0xE0 | (c >> 12));
        b[n++] = (char)(0x80 | ((c >> 6) & 0x3F));
        b[n++] = (char)(0x80 | (c & 0x3F));
    }
    else
    {
        b[n++] = (char)(0xF0 | (c >> 18));
        b[n++] = (char)(0x80 | ((c >> 12) & 0x3F));
        b[n++] = (char)(0x80 | ((c >> 6) & 0x3F));
        b[n++] = (char)(0x80 | (c & 0x3F));
    }
    /* Chr$(0) is a one-byte string holding NUL, as in VB. */
    tf_string *out = tf_str_alloc(n);
    memcpy(out->data, b, (size_t)n);
    return out;
}

int32_t tf_str_asc(const tf_string *s)
{
    if (length_of(s) == 0)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
        return 0;
    }
    const unsigned char *b = (const unsigned char *)bytes_of(s);
    const int32_t n = advance(s, 0, 1);
    uint32_t c = n == 1 ? b[0] : (uint32_t)b[0] & (0x7Fu >> n);
    for (int32_t i = 1; i < n; ++i)
    {
        c = (c << 6) | (b[i] & 0x3Fu);
    }
    return (int32_t)c;
}

tf_string *tf_str_replace(const tf_string *s, const tf_string *find, const tf_string *with)
{
    const int32_t flen = length_of(find);
    const int32_t wlen = length_of(with);
    const int32_t slen = length_of(s);
    if (flen == 0)
    {
        return slice(s, 0, slen);
    }
    int64_t count = 0;
    for (int32_t at = search(s, find, 0, slen, 0); at >= 0;
         at = search(s, find, at + flen, slen, 0))
    {
        ++count;
    }
    tf_string *out = make(slen + count * (wlen - flen));
    if (out == NULL)
    {
        return NULL;
    }
    char *o = out->data;
    int32_t done = 0;
    for (int32_t at = search(s, find, 0, slen, 0); at >= 0;
         at = search(s, find, at + flen, slen, 0))
    {
        memcpy(o, bytes_of(s) + done, (size_t)(at - done));
        o += at - done;
        memcpy(o, bytes_of(with), (size_t)wlen);
        o += wlen;
        done = at + flen;
    }
    memcpy(o, bytes_of(s) + done, (size_t)(slen - done));
    return out;
}

tf_string *tf_str_reverse(const tf_string *s)
{
    const int32_t length = length_of(s);
    tf_string *out = make(length);
    for (int32_t from = 0; from < length;)
    {
        const int32_t to = advance(s, from, 1);
        memcpy(out->data + (length - to), bytes_of(s) + from, (size_t)(to - from));
        from = to;
    }
    return out;
}

void tf_str_split(tf_array *to, const tf_string *s, const tf_string *delimiter)
{
    if (to->locks > 0 || to->fixed)
    {
        tf_raise(TF_ERR_ARRAY_LOCKED, NULL);
        return;
    }
    const int32_t slen = length_of(s);
    const int32_t dlen = length_of(delimiter);
    int32_t pieces = 0;
    if (slen > 0)
    {
        pieces = 1;
        for (int32_t at = dlen > 0 ? search(s, delimiter, 0, slen, 0) : -1; at >= 0;
             at = search(s, delimiter, at + dlen, slen, 0))
        {
            ++pieces;
        }
    }
    /* Built aside, then moved in: s may be an element of to itself. */
    tf_array parts = {0};
    const int32_t upper = pieces - 1;
    tf_array_redim(&parts, TF_ELEM_STRING, NULL, 0, 1, &upper);
    int32_t from = 0;
    for (int32_t i = 0; i < pieces; ++i)
    {
        int32_t at = dlen > 0 ? search(s, delimiter, from, slen, 0) : -1;
        if (at < 0)
        {
            at = slen;
        }
        *(tf_string **)tf_array_at1(&parts, i) = slice(s, from, at);
        from = at + dlen;
    }
    tf_array_free(to);
    *to = parts;
}

tf_string *tf_str_join(const tf_array *strings, const tf_string *delimiter)
{
    tf_string *const *items = strings->data;
    const int64_t count = strings->count;
    int64_t total = 0;
    for (int64_t i = 0; i < count; ++i)
    {
        total += length_of(items[i]) + (i > 0 ? length_of(delimiter) : 0);
    }
    tf_string *out = make(total);
    char *o = out != NULL ? out->data : NULL;
    for (int64_t i = 0; o != NULL && i < count; ++i)
    {
        if (i > 0)
        {
            memcpy(o, bytes_of(delimiter), (size_t)length_of(delimiter));
            o += length_of(delimiter);
        }
        memcpy(o, bytes_of(items[i]), (size_t)length_of(items[i]));
        o += length_of(items[i]);
    }
    return out;
}
