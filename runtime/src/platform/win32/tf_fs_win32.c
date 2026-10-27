/* SPDX-License-Identifier: 0BSD */
/*
 * tf_fs_win32.c -- the file system on Windows: the runtime's UTF-8 paths
 * become UTF-16 for the wide-character functions, so any file name works
 * whatever the ANSI code page; positions use the 64-bit _fseeki64.
 */
#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include "../../tf_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <windows.h>

/* path as UTF-16, to free; NULL (errno EINVAL) if it is not UTF-8. */
static wchar_t *wide(const char *path)
{
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (n == 0)
    {
        errno = EINVAL;
        return NULL;
    }
    wchar_t *w = tf_xmalloc((size_t)n * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, w, n);
    return w;
}

FILE *tf_platform_fopen(const char *path, const char *mode)
{
    wchar_t *p = wide(path);
    wchar_t *m = wide(mode);
    FILE *f = p != NULL && m != NULL ? _wfopen(p, m) : NULL;
    free(p);
    free(m);
    return f;
}

int tf_platform_fseek(FILE *f, int64_t offset, int origin)
{
    return _fseeki64(f, offset, origin);
}

int64_t tf_platform_ftell(FILE *f)
{
    return _ftelli64(f);
}

#else
/* ISO C forbids an empty translation unit. */
typedef int tf_fs_win32_not_used;
#endif
