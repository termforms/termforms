/* SPDX-License-Identifier: 0BSD */
/*
 * tf_fs_win32.c -- the file system on Windows: the runtime's UTF-8 paths
 * become UTF-16 for the wide-character functions, so any file name works
 * whatever the ANSI code page; positions use the 64-bit _fseeki64.
 */
#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include "../../tf_internal.h"

#include <direct.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <wchar.h>
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

/* Runs op on path as UTF-16. */
static int with_wide(const char *path, int (*op)(const wchar_t *))
{
    wchar_t *w = wide(path);
    const int result = w != NULL ? op(w) : -1;
    free(w);
    return result;
}

static int wmkdir(const wchar_t *p)
{
    return _wmkdir(p);
}

int tf_platform_remove(const char *path)
{
    return with_wide(path, _wremove);
}

int tf_platform_mkdir(const char *path)
{
    return with_wide(path, wmkdir);
}

int tf_platform_rmdir(const char *path)
{
    return with_wide(path, _wrmdir);
}

int tf_platform_chdir(const char *path)
{
    return with_wide(path, _wchdir);
}

int tf_platform_rename(const char *from, const char *to)
{
    wchar_t *f = wide(from);
    wchar_t *t = wide(to);
    /* _wrename fails with EEXIST if to is there: VB's Name does not replace. */
    const int result = f != NULL && t != NULL ? _wrename(f, t) : -1;
    free(f);
    free(t);
    return result;
}

int tf_platform_stat(const char *path, int64_t *size, int64_t *modified, int *folder)
{
    wchar_t *w = wide(path);
    struct _stat64 st;
    const int result = w != NULL ? _wstat64(w, &st) : -1;
    free(w);
    if (result != 0)
    {
        return -1;
    }
    *size = (int64_t)st.st_size;
    *modified = (int64_t)st.st_mtime;
    *folder = (st.st_mode & _S_IFDIR) != 0;
    return 0;
}

/* UTF-16 to UTF-8, to free. */
static char *narrow(const wchar_t *w)
{
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *s = tf_xmalloc((size_t)(n > 0 ? n : 1));
    s[0] = '\0';
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
    return s;
}

char *tf_platform_getcwd(void)
{
    wchar_t *w = _wgetcwd(NULL, 0);
    if (w == NULL)
    {
        return NULL;
    }
    char *s = narrow(w);
    free(w);
    return s;
}

int tf_platform_list(const char *dir, void (*each)(const char *name, int folder, void *context),
                     void *context)
{
    const char *base = dir[0] != '\0' ? dir : "."; /* "" is the current folder */
    char *pattern = tf_xmalloc(strlen(base) + 3);
    strcpy(pattern, base);
    strcat(pattern, "\\*");
    wchar_t *w = wide(pattern);
    free(pattern);
    if (w == NULL)
    {
        return -1;
    }
    WIN32_FIND_DATAW found;
    HANDLE h = FindFirstFileW(w, &found);
    free(w);
    if (h == INVALID_HANDLE_VALUE)
    {
        errno = GetLastError() == ERROR_ACCESS_DENIED ? EACCES : ENOENT;
        return -1;
    }
    do
    {
        if (wcscmp(found.cFileName, L".") == 0 || wcscmp(found.cFileName, L"..") == 0)
        {
            continue;
        }
        char *name = narrow(found.cFileName);
        each(name, (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0, context);
        free(name);
    } while (FindNextFileW(h, &found));
    FindClose(h);
    return 0;
}

void tf_platform_local_time(int64_t seconds, int32_t fields[6])
{
    const __time64_t t = (__time64_t)seconds;
    struct tm tm;
    _localtime64_s(&tm, &t);
    fields[0] = tm.tm_year + 1900;
    fields[1] = tm.tm_mon + 1;
    fields[2] = tm.tm_mday;
    fields[3] = tm.tm_hour;
    fields[4] = tm.tm_min;
    fields[5] = tm.tm_sec;
}

#else
/* ISO C forbids an empty translation unit. */
typedef int tf_fs_win32_not_used;
#endif
