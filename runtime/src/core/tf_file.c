/* SPDX-License-Identifier: 0BSD */
/*
 * tf_file.c -- numbered files (see "Files" in tf_runtime.h), and standard
 * input, output and error for console programs (Print, Print #0, Line
 * Input, EOF(0)). Every file is opened in C's binary mode, so the runtime
 * decides line ends itself: LF written on every platform, LF or CRLF read.
 */
#include "../tf_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct open_file
{
    FILE *f;
    int32_t mode;  /* TF_FILE_INPUT ..., or 0 when the number is free */
    int32_t wrote; /* Binary: the last access wrote (C needs a seek before reading) */
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
    files[number].wrote = 0;
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

static void console_write(int stream, const tf_string *text, int32_t newline);

void tf_file_print(int32_t number, const tf_string *text, int32_t newline)
{
    if (number == 0) /* Print #0: standard error */
    {
        console_write(2, text, newline);
        return;
    }
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

static int console_eof(void);

int tf_file_eof(int32_t number)
{
    if (number == 0)
    {
        return console_eof();
    }
    open_file *of = file_for(number);
    if (of == NULL)
    {
        return 1;
    }
    if (of->mode == TF_FILE_OUTPUT || of->mode == TF_FILE_APPEND)
    {
        return 1; /* writing: always at the end */
    }
    if (of->mode == TF_FILE_BINARY)
    {
        return tf_platform_ftell(of->f) >= tf_file_lof(number);
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

/* The Binary file for number positioned for a read or write at offset (or
   where it is); NULL after an error. C requires a seek between writing and
   reading, so one is made whenever the direction changes. */
static open_file *binary_at(int32_t number, int32_t has_offset, int64_t offset, int writing)
{
    open_file *of = file_for(number);
    if (of == NULL)
    {
        return NULL;
    }
    if (of->mode != TF_FILE_BINARY)
    {
        tf_raise(TF_ERR_BAD_FILE_MODE, NULL);
        return NULL;
    }
    if (has_offset && offset < 0)
    {
        tf_raise(TF_ERR_BAD_RECORD, NULL);
        return NULL;
    }
    if (has_offset ? tf_platform_fseek(of->f, offset, SEEK_SET) != 0
                   : of->wrote != writing && tf_platform_fseek(of->f, 0, SEEK_CUR) != 0)
    {
        tf_raise(os_error(writing), NULL);
        return NULL;
    }
    of->wrote = writing;
    return of;
}

void tf_file_get(int32_t number, int32_t has_offset, int64_t offset, tf_array *buffer,
                 int32_t has_count, int32_t count)
{
    if (has_count && count < 0)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
        return;
    }
    open_file *of = binary_at(number, has_offset, offset, 0);
    if (of == NULL)
    {
        return;
    }
    if (has_count)
    {
        const int32_t upper = count - 1;
        tf_array_redim(buffer, TF_ELEM_U8, NULL, 0, 1, &upper);
        if (tf_error_pending)
        {
            return; /* the buffer is locked (passed ByRef meanwhile) */
        }
    }
    const size_t wanted = (size_t)buffer->count;
    const size_t got = wanted > 0 ? fread(buffer->data, 1, wanted, of->f) : 0;
    if (got < wanted && ferror(of->f))
    {
        tf_raise(TF_ERR_DEVICE_IO, NULL);
        return;
    }
    if (got < wanted)
    {
        if (buffer->fixed)
        {
            memset((unsigned char *)buffer->data + got, 0, wanted - got);
        }
        else
        {
            const int32_t upper = (int32_t)got - 1;
            tf_array_redim(buffer, TF_ELEM_U8, NULL, 1, 1, &upper);
        }
    }
}

void tf_file_put(int32_t number, int32_t has_offset, int64_t offset, const tf_array *buffer,
                 int32_t has_count, int32_t count)
{
    if (has_count && count < 0)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
        return;
    }
    if (has_count && count > buffer->count)
    {
        tf_raise(TF_ERR_SUBSCRIPT, NULL);
        return;
    }
    open_file *of = binary_at(number, has_offset, offset, 1);
    if (of == NULL)
    {
        return;
    }
    const size_t n = (size_t)(has_count ? count : buffer->count);
    if (n > 0 && fwrite(buffer->data, 1, n, of->f) != n)
    {
        tf_raise(os_error(1), NULL);
    }
}

void tf_file_seek(int32_t number, int64_t offset)
{
    open_file *of = file_for(number);
    if (of == NULL)
    {
        return;
    }
    if (offset < 0)
    {
        tf_raise(TF_ERR_BAD_RECORD, NULL);
        return;
    }
    if (tf_platform_fseek(of->f, offset, SEEK_SET) != 0)
    {
        tf_raise(os_error(0), NULL);
    }
}

/* -- Standard input and output ---------------------------------------------
 *
 * While a form or dialog is on screen the terminal is in raw mode on the
 * alternate screen, so text written then would be drawn over the form and
 * lost when it closes. It is kept instead, each piece with its stream, and
 * written in order once the forms layer says the terminal is normal.
 */

typedef struct held_text
{
    int stream;
    char *data;
    size_t size;
} held_text;

static int screen_in_use = 0;
static held_text *held = NULL;
static size_t held_count = 0;
static size_t held_capacity = 0;

/* A line read ahead by EOF(0), given to the next Line Input. */
static char *ahead = NULL;
static size_t ahead_size = 0;
static int ahead_ready = 0; /* ahead holds a line, or (NULL) the input ended */

static void emit(int stream, const char *data, size_t size)
{
    if (screen_in_use)
    {
        if (held_count == held_capacity)
        {
            held_capacity = held_capacity == 0 ? 16 : held_capacity * 2;
            held = tf_xrealloc(held, held_capacity * sizeof(held_text));
        }
        char *copy = tf_xmalloc(size > 0 ? size : 1);
        memcpy(copy, data, size);
        held[held_count].stream = stream;
        held[held_count].data = copy;
        held[held_count].size = size;
        ++held_count;
        return;
    }
    tf_platform_console_write(stream, data, size);
}

void tf_console_screen(int in_use)
{
    screen_in_use = in_use;
    if (in_use)
    {
        return;
    }
    for (size_t i = 0; i < held_count; ++i)
    {
        tf_platform_console_write(held[i].stream, held[i].data, held[i].size);
        free(held[i].data);
    }
    held_count = 0;
}

static void console_write(int stream, const tf_string *text, int32_t newline)
{
    const size_t length = (size_t)tf_str_lenb(text);
    if (length > 0)
    {
        emit(stream, tf_str_cstr(text), length);
    }
    if (newline)
    {
        emit(stream, "\n", 1);
    }
}

void tf_console_print(const tf_string *text, int32_t newline)
{
    console_write(1, text, newline);
}

/* Reads the next line into ahead, unless it is there already. */
static void read_ahead(void)
{
    if (!ahead_ready)
    {
        if (tf_platform_console_read_line(&ahead, &ahead_size) != 0)
        {
            ahead = NULL;
        }
        ahead_ready = 1;
    }
}

static int console_eof(void)
{
    if (screen_in_use)
    {
        tf_raise(TF_ERR_TERMINAL, NULL);
        return 1;
    }
    read_ahead();
    return ahead == NULL;
}

tf_string *tf_console_line_input(const tf_string *prompt)
{
    if (screen_in_use)
    {
        tf_raise(TF_ERR_TERMINAL, NULL);
        return NULL;
    }
    console_write(1, prompt, 0);
    read_ahead();
    ahead_ready = 0;
    if (ahead == NULL)
    {
        tf_raise(TF_ERR_PAST_END, NULL);
        return NULL;
    }
    tf_string *s = text_of((const unsigned char *)ahead, ahead_size);
    free(ahead);
    ahead = NULL;
    return s;
}
