/* SPDX-License-Identifier: 0BSD */
/*
 * tf_fsys.c -- Kill, MkDir, Dir$ and the other file-system routines (see
 * tf_runtime.h). The operating system's part is the platform layer; this
 * file gives VB's behaviour on top of it: wildcards, Dir$'s one-name-per-
 * call listing, VB6's error numbers.
 */
#include "../tf_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* What went wrong as VB6's number. missing: what ENOENT means here, 53
   (the file) or 76 (a folder on the way). */
static int32_t fs_error(int32_t missing)
{
    switch (errno)
    {
        case ENOENT:
        case ENOTDIR:
            return missing;
        case EACCES:
        case EPERM:
        case EROFS:
        case EBUSY:
            return TF_ERR_PERMISSION;
        case EEXIST:
            return TF_ERR_FILE_EXISTS;
        case ENOSPC:
            return TF_ERR_DISK_FULL;
        default:
            return TF_ERR_PATH_ACCESS; /* not empty, and the rest */
    }
}

static const char *path_of(const tf_string *s)
{
    return tf_str_cstr(s);
}

static int is_separator(char c)
{
    return c == '/' || (TF_ON_WINDOWS && c == '\\');
}

/* The offset just after the last separator: the folder is before it. */
static size_t name_start(const char *path)
{
    size_t start = 0;
    for (size_t i = 0; path[i] != '\0'; ++i)
    {
        if (is_separator(path[i]) || (TF_ON_WINDOWS && path[i] == ':'))
        {
            start = i + 1;
        }
    }
    return start;
}

static int same_char(char a, char b)
{
    if (TF_NAMES_IGNORE_CASE)
    {
        a = (a >= 'A' && a <= 'Z') ? (char)(a - 'A' + 'a') : a;
        b = (b >= 'A' && b <= 'Z') ? (char)(b - 'A' + 'a') : b;
    }
    return a == b;
}

/* * matches any run of characters, ? any one (a whole UTF-8 character). */
static int matches(const char *pattern, const char *name)
{
    if (*pattern == '\0')
    {
        return *name == '\0';
    }
    if (*pattern == '*')
    {
        for (const char *rest = name;; ++rest)
        {
            if (matches(pattern + 1, rest))
            {
                return 1;
            }
            if (*rest == '\0')
            {
                return 0;
            }
        }
    }
    if (*name == '\0')
    {
        return 0;
    }
    if (*pattern == '?')
    {
        ++name;
        while (((unsigned char)*name & 0xC0u) == 0x80u)
        {
            ++name;
        }
        return matches(pattern + 1, name);
    }
    return same_char(*pattern, *name) && matches(pattern + 1, name + 1);
}

/* The files of a folder that match a pattern, sorted (so a program sees
   the same order on every system). */
typedef struct name_list
{
    char **names;
    size_t count;
    size_t capacity;
    const char *pattern;
} name_list;

static void collect(const char *name, int folder, void *context)
{
    name_list *list = context;
    if (folder || !matches(list->pattern, name))
    {
        return;
    }
    if (list->count == list->capacity)
    {
        list->capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        list->names = tf_xrealloc(list->names, list->capacity * sizeof *list->names);
    }
    list->names[list->count] = tf_xmalloc(strlen(name) + 1);
    strcpy(list->names[list->count++], name);
}

static int by_name(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void free_list(name_list *list)
{
    for (size_t i = 0; i < list->count; ++i)
    {
        free(list->names[i]);
    }
    free(list->names);
    memset(list, 0, sizeof *list);
}

/* Lists path's folder for its last part; *folder_out (to free) is the
   folder with its separator, for making full names. -1 if unreadable. */
static int list_matches(const char *path, name_list *list, char **folder_out)
{
    const size_t start = name_start(path);
    char *folder = tf_xmalloc(start + 1);
    memcpy(folder, path, start);
    folder[start] = '\0';
    list->pattern = path + start;
    /* The platform lists "dir", not "dir/" (except a root such as "/"). */
    char *dir = tf_xmalloc(start + 1);
    memcpy(dir, path, start);
    dir[start > 1 && is_separator(path[start - 1]) ? start - 1 : start] = '\0';
    const int result = tf_platform_list(dir, collect, list);
    free(dir);
    if (result == 0 && list->count > 1)
    {
        qsort(list->names, list->count, sizeof *list->names, by_name);
    }
    *folder_out = folder;
    return result;
}

void tf_fs_kill(const tf_string *pattern)
{
    name_list list = {0};
    char *folder = NULL;
    if (list_matches(path_of(pattern), &list, &folder) != 0)
    {
        tf_raise(fs_error(TF_ERR_PATH_NOT_FOUND), NULL);
    }
    else if (list.count == 0)
    {
        tf_raise(TF_ERR_FILE_NOT_FOUND, NULL);
    }
    for (size_t i = 0; i < list.count && !tf_error_pending; ++i)
    {
        char *full = tf_xmalloc(strlen(folder) + strlen(list.names[i]) + 1);
        strcpy(full, folder);
        strcat(full, list.names[i]);
        if (tf_platform_remove(full) != 0)
        {
            tf_raise(fs_error(TF_ERR_FILE_NOT_FOUND), NULL);
        }
        free(full);
    }
    free(folder);
    free_list(&list);
}

void tf_fs_mkdir(const tf_string *path)
{
    if (tf_platform_mkdir(path_of(path)) != 0)
    {
        tf_raise(errno == EEXIST ? TF_ERR_PATH_ACCESS : fs_error(TF_ERR_PATH_NOT_FOUND), NULL);
    }
}

void tf_fs_rmdir(const tf_string *path)
{
    if (tf_platform_rmdir(path_of(path)) != 0)
    {
        tf_raise(errno == EEXIST ? TF_ERR_PATH_ACCESS : fs_error(TF_ERR_PATH_NOT_FOUND), NULL);
    }
}

void tf_fs_chdir(const tf_string *path)
{
    if (tf_platform_chdir(path_of(path)) != 0)
    {
        tf_raise(fs_error(TF_ERR_PATH_NOT_FOUND), NULL);
    }
}

tf_string *tf_fs_curdir(void)
{
    char *dir = tf_platform_getcwd();
    if (dir == NULL)
    {
        tf_raise(fs_error(TF_ERR_PATH_NOT_FOUND), NULL);
        return NULL;
    }
    tf_string *s = tf_str_from_utf8(dir, (int32_t)strlen(dir));
    free(dir);
    return s;
}

void tf_fs_name(const tf_string *from, const tf_string *to)
{
    int64_t size = 0;
    int64_t modified = 0;
    int folder = 0;
    if (tf_platform_stat(path_of(from), &size, &modified, &folder) != 0)
    {
        tf_raise(fs_error(TF_ERR_FILE_NOT_FOUND), NULL);
        return;
    }
    if (tf_platform_rename(path_of(from), path_of(to)) != 0)
    {
        /* from is there, so a missing piece is to's folder. */
        tf_raise(fs_error(TF_ERR_PATH_NOT_FOUND), NULL);
    }
}

void tf_fs_copy(const tf_string *from, const tf_string *to)
{
    FILE *in = tf_platform_fopen(path_of(from), "rb");
    if (in == NULL)
    {
        tf_raise(fs_error(TF_ERR_FILE_NOT_FOUND), NULL);
        return;
    }
    FILE *out = tf_platform_fopen(path_of(to), "wb");
    if (out == NULL)
    {
        tf_raise(fs_error(TF_ERR_PATH_NOT_FOUND), NULL);
        fclose(in);
        return;
    }
    char *buffer = tf_xmalloc(65536);
    size_t n;
    int failed = 0;
    while (!failed && (n = fread(buffer, 1, 65536, in)) > 0)
    {
        failed = fwrite(buffer, 1, n, out) != n;
    }
    failed = failed || ferror(in);
    free(buffer);
    fclose(in);
    if (fclose(out) != 0 || failed)
    {
        tf_raise(fs_error(TF_ERR_DEVICE_IO), NULL);
    }
}

int64_t tf_fs_len(const tf_string *path)
{
    int64_t size = 0;
    int64_t modified = 0;
    int folder = 0;
    if (tf_platform_stat(path_of(path), &size, &modified, &folder) != 0)
    {
        tf_raise(fs_error(TF_ERR_FILE_NOT_FOUND), NULL);
        return 0;
    }
    return size;
}

tf_string *tf_fs_datetime(const tf_string *path)
{
    int64_t size = 0;
    int64_t modified = 0;
    int folder = 0;
    if (tf_platform_stat(path_of(path), &size, &modified, &folder) != 0)
    {
        tf_raise(fs_error(TF_ERR_FILE_NOT_FOUND), NULL);
        return NULL;
    }
    int32_t f[6];
    tf_platform_local_time(modified, f);
    char text[32];
    snprintf(text, sizeof text, "%04d-%02d-%02d %02d:%02d:%02d", (int)f[0], (int)f[1], (int)f[2],
             (int)f[3], (int)f[4], (int)f[5]);
    return tf_str_from_utf8(text, (int32_t)strlen(text));
}

int tf_fs_exists(const tf_string *path)
{
    int64_t size = 0;
    int64_t modified = 0;
    int folder = 0;
    return tf_str_lenb(path) > 0 && tf_platform_stat(path_of(path), &size, &modified, &folder) == 0;
}

/* Dir$'s listing between calls. */
static name_list dir_list = {0};
static size_t dir_next = 0;
static int dir_started = 0;

tf_string *tf_fs_dir(const tf_string *pattern)
{
    free_list(&dir_list);
    dir_next = 0;
    dir_started = 1;
    char *folder = NULL;
    (void)list_matches(path_of(pattern), &dir_list, &folder); /* unreadable: no names, as VB */
    free(folder);
    return tf_fs_dir_next();
}

tf_string *tf_fs_dir_next(void)
{
    if (!dir_started)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL); /* Dir$() before any Dir$(pattern) */
        return NULL;
    }
    if (dir_next >= dir_list.count)
    {
        return NULL;
    }
    const char *name = dir_list.names[dir_next++];
    return tf_str_from_utf8(name, (int32_t)strlen(name));
}
