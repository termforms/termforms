/* SPDX-License-Identifier: 0BSD */
/*
 * tf_c.c -- what a Declare'd C function's call needs: Strings as C's char
 * pointers (and a ByRef String as a buffer C may fill), C's long, unsigned
 * long and size_t checked against a Long, and the Platform value.
 *
 * C functions cannot raise TermForms errors; these conversions can, before
 * the call is made, so a value that does not fit never reaches C cut short.
 */
#include "../tf_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

char *tf_c_buffer(const tf_string *s)
{
    const int32_t length = tf_str_lenb(s);
    char *buffer = tf_xmalloc((size_t)length + 1);
    memcpy(buffer, tf_str_cstr(s), (size_t)length);
    buffer[length] = '\0';
    return buffer;
}

/* The String becomes what C left in the buffer up to its first NUL; the
   buffer is freed and its pointer cleared (the statement's cleanup frees
   it otherwise, if something failed before the call). */
void tf_c_unbuffer(tf_string **slot, char **buffer)
{
    const int32_t capacity = tf_str_lenb(*slot);
    const char *end = memchr(*buffer, '\0', (size_t)capacity + 1);
    const int32_t length = end != NULL ? (int32_t)(end - *buffer) : capacity;
    tf_str_assign(slot, tf_str_from_utf8(*buffer, length));
    free(*buffer);
    *buffer = NULL;
}

void tf_c_free(char *buffer)
{
    free(buffer);
}

tf_string *tf_str_from_c(const char *s)
{
    if (s == NULL)
    {
        return NULL; /* "" */
    }
    const size_t length = strlen(s);
    if (length > TF_STR_MAX_LENGTH)
    {
        tf_raise(TF_ERR_OUT_OF_STRING_SPACE, NULL);
        return NULL;
    }
    return tf_str_from_utf8(s, (int32_t)length);
}

long tf_c_long(int64_t value)
{
    if (value < LONG_MIN || value > LONG_MAX) /* only where long is 32-bit (Windows) */
    {
        tf_raise(TF_ERR_OVERFLOW, NULL);
        return 0;
    }
    return (long)value;
}

unsigned long tf_c_ulong(int64_t value)
{
    if (value < 0 || (uint64_t)value > ULONG_MAX)
    {
        tf_raise(TF_ERR_OVERFLOW, NULL);
        return 0;
    }
    return (unsigned long)value;
}

size_t tf_c_size(int64_t value)
{
    if (value < 0 || (uint64_t)value > SIZE_MAX)
    {
        tf_raise(TF_ERR_OVERFLOW, NULL);
        return 0;
    }
    return (size_t)value;
}

int64_t tf_c_from_ulong(unsigned long value)
{
    if ((uint64_t)value > (uint64_t)INT64_MAX)
    {
        tf_raise(TF_ERR_OVERFLOW, NULL);
        return 0;
    }
    return (int64_t)value;
}

int64_t tf_c_from_size(size_t value)
{
    if ((uint64_t)value > (uint64_t)INT64_MAX)
    {
        tf_raise(TF_ERR_OVERFLOW, NULL);
        return 0;
    }
    return (int64_t)value;
}

/* Set by the C compiler for the target, so a program cross-built on a Mac
   for a Linux server says Linux. */
tf_string *tf_platform_name(void)
{
#if defined(_WIN32)
    static const char kName[] = "Windows";
#elif defined(__APPLE__)
    static const char kName[] = "macOS";
#elif defined(__linux__)
    static const char kName[] = "Linux";
#else
    static const char kName[] = "Other";
#endif
    return tf_str_from_utf8(kName, (int32_t)(sizeof kName - 1));
}
