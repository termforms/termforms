/* SPDX-License-Identifier: 0BSD */
/*
 * tf_file.c -- numbered files (see "Files" in tf_runtime.h). Every file is
 * opened in C's binary mode, so the runtime decides line ends itself: LF
 * written on every platform, LF or CRLF read.
 */
#include "../tf_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct open_file
{
    FILE *f;
    int32_t mode; /* TF_FILE_INPUT ..., or 0 when the number is free */
} open_file;

/* One program, one thread, as the rest of the runtime. */
static open_file files[TF_FILE_MAX + 1];

/* The open file for number, or NULL after error 52. */
static open_file *file_for(int32_t number)
{
    if (number < 1 || number > TF_FILE_MAX || files[number].f == NULL)
    {
        tf_raise(TF_ERR_BAD_FILE, NULL);
        return NULL;
    }
    return &files[number];
}

/* What went wrong as VB6's number, from errno after a failed open or I/O.
   A missing file when only reading means the file; when creating one, the
   folder it would go in. */
static int32_t os_error(int creating)
{
    switch (errno)
    {
        case ENOENT:
            return creating ? TF_ERR_PATH_NOT_FOUND : TF_ERR_FILE_NOT_FOUND;
        case EACCES:
        case EPERM:
        case EROFS:
            return TF_ERR_PERMISSION;
        case EMFILE:
        case ENFILE:
            return TF_ERR_TOO_MANY_FILES;
        case ENOSPC:
            return TF_ERR_DISK_FULL;
        case EIO:
            return TF_ERR_DEVICE_IO;
        default:
            return TF_ERR_PATH_ACCESS;
    }
}

void tf_file_open(const tf_string *path, int32_t mode, int32_t number)
{
    if (number < 1 || number > TF_FILE_MAX)
    {
        tf_raise(TF_ERR_BAD_FILE, NULL);
        return;
    }
    if (files[number].f != NULL)
    {
        tf_raise(TF_ERR_FILE_OPEN, NULL);
        return;
    }
    const char *name = tf_str_cstr(path);
    if (name[0] == '\0')
    {
        tf_raise(TF_ERR_BAD_FILE, NULL);
        return;
    }
    FILE *f = NULL;
    switch (mode)
    {
        case TF_FILE_INPUT:
            f = tf_platform_fopen(name, "rb");
            break;
        case TF_FILE_OUTPUT:
            f = tf_platform_fopen(name, "wb");
            break;
        case TF_FILE_APPEND:
            f = tf_platform_fopen(name, "ab");
            break;
        default: /* Binary: read and write; made if it is not there, as VB */
            f = tf_platform_fopen(name, "r+b");
            if (f == NULL && errno == ENOENT)
            {
                f = tf_platform_fopen(name, "w+b");
            }
            break;
    }
    if (f == NULL)
    {
        tf_raise(os_error(mode != TF_FILE_INPUT), NULL);
        return;
    }
    files[number].f = f;
    files[number].mode = mode;
}

void tf_file_close(int32_t number)
{
    if (number < 1 || number > TF_FILE_MAX)
    {
        tf_raise(TF_ERR_BAD_FILE, NULL);
        return;
    }
    if (files[number].f != NULL)
    {
        /* A write still buffered can fail here (a full disk). */
        const int failed = fclose(files[number].f) != 0;
        files[number].f = NULL;
        files[number].mode = 0;
        if (failed)
        {
            tf_raise(os_error(1), NULL);
        }
    }
}

void tf_file_close_all(void)
{
    for (int32_t n = 1; n <= TF_FILE_MAX; ++n)
    {
        if (files[n].f != NULL)
        {
            tf_file_close(n);
        }
    }
}

int32_t tf_file_free(void)
{
    for (int32_t n = 1; n <= TF_FILE_MAX; ++n)
    {
        if (files[n].f == NULL)
        {
            return n;
        }
    }
    tf_raise(TF_ERR_TOO_MANY_FILES, NULL);
    return 0;
}

void tf_file_print(int32_t number, const tf_string *text, int32_t newline)
{
    open_file *of = file_for(number);
    if (of == NULL)
    {
        return;
    }
    if (of->mode != TF_FILE_OUTPUT && of->mode != TF_FILE_APPEND)
    {
        tf_raise(TF_ERR_BAD_FILE_MODE, NULL);
        return;
    }
    const size_t length = (size_t)tf_str_lenb(text);
    if ((length > 0 && fwrite(tf_str_cstr(text), 1, length, of->f) != length) ||
        (newline && fputc('\n', of->f) == EOF))
    {
        tf_raise(os_error(1), NULL);
    }
}

/* The length of the valid UTF-8 sequence starting at b (at most n bytes
   left), or 0 if it is not one: an overlong form, a surrogate and anything
   above U+10FFFF are not. */
static int utf8_length(const unsigned char *b, size_t n)
{
    if (b[0] < 0x80)
    {
        return 1;
    }
    int length = b[0] >= 0xF0 ? 4 : b[0] >= 0xE0 ? 3 : b[0] >= 0xC2 ? 2 : 0;
    if (length == 0 || b[0] > 0xF4 || (size_t)length > n)
    {
        return 0;
    }
    for (int i = 1; i < length; ++i)
    {
        if ((b[i] & 0xC0) != 0x80)
        {
            return 0;
        }
    }
    if ((b[0] == 0xE0 && b[1] < 0xA0) || (b[0] == 0xED && b[1] >= 0xA0) ||
        (b[0] == 0xF0 && b[1] < 0x90) || (b[0] == 0xF4 && b[1] >= 0x90))
    {
        return 0;
    }
    return length;
}

/* A line's bytes as a string, each invalid byte replaced by U+FFFD, so a
   file in another encoding cannot make a malformed string. */
static tf_string *text_of(const unsigned char *b, size_t n)
{
    size_t bad = 0;
    for (size_t i = 0; i < n;)
    {
        const int length = utf8_length(b + i, n - i);
        bad += length == 0;
        i += length == 0 ? 1 : (size_t)length;
    }
    if (n + bad * 2 > TF_STR_MAX_LENGTH)
    {
        tf_raise(TF_ERR_OUT_OF_STRING_SPACE, NULL);
        return NULL;
    }
    if (n == 0)
    {
        return NULL;
    }
    tf_string *s = tf_str_alloc((int32_t)(n + bad * 2));
    char *o = s->data;
    for (size_t i = 0; i < n;)
    {
        const int length = utf8_length(b + i, n - i);
        if (length == 0)
        {
            memcpy(o, "\xEF\xBF\xBD", 3);
            o += 3;
            ++i;
            continue;
        }
        memcpy(o, b + i, (size_t)length);
        o += length;
        i += (size_t)length;
    }
    return s;
}

tf_string *tf_file_line_input(int32_t number)
{
    open_file *of = file_for(number);
    if (of == NULL)
    {
        return NULL;
    }
    if (of->mode != TF_FILE_INPUT)
    {
        tf_raise(TF_ERR_BAD_FILE_MODE, NULL);
        return NULL;
    }
    size_t size = 0;
    size_t capacity = 128;
    unsigned char *line = tf_xmalloc(capacity);
    int c = getc(of->f);
    if (c == EOF)
    {
        free(line);
        tf_raise(ferror(of->f) ? TF_ERR_DEVICE_IO : TF_ERR_PAST_END, NULL);
        return NULL;
    }
    for (; c != EOF && c != '\n'; c = getc(of->f))
    {
        if (size == capacity)
        {
            capacity *= 2;
            line = tf_xrealloc(line, capacity);
        }
        line[size++] = (unsigned char)c;
    }
    if (size > 0 && line[size - 1] == '\r')
    {
        --size; /* a CRLF line end */
    }
    tf_string *s = text_of(line, size);
    free(line);
    return s;
}

int tf_file_eof(int32_t number)
{
    open_file *of = file_for(number);
    if (of == NULL)
    {
        return 1;
    }
    if (of->mode == TF_FILE_OUTPUT || of->mode == TF_FILE_APPEND)
    {
        return 1; /* writing: always at the end */
    }
    const int c = getc(of->f);
    if (c == EOF)
    {
        return 1;
    }
    ungetc(c, of->f);
    return 0;
}

int64_t tf_file_lof(int32_t number)
{
    open_file *of = file_for(number);
    if (of == NULL)
    {
        return 0;
    }
    fflush(of->f);
    const int64_t here = tf_platform_ftell(of->f);
    tf_platform_fseek(of->f, 0, SEEK_END);
    const int64_t length = tf_platform_ftell(of->f);
    tf_platform_fseek(of->f, here, SEEK_SET);
    return length;
}

int64_t tf_file_seek_pos(int32_t number)
{
    open_file *of = file_for(number);
    return of != NULL ? tf_platform_ftell(of->f) : 0;
}
