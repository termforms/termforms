/* SPDX-License-Identifier: 0BSD */
/*
 * tf_sys_win32.c -- the system on Windows: commands through cmd.exe as
 * UTF-16 (_wsystem), the precise system clock, and the command line's
 * arguments for Command$.
 */
#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include "../../tf_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

static char *narrow(const wchar_t *w)
{
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *s = tf_xmalloc((size_t)(n > 0 ? n : 1));
    s[0] = '\0';
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
    return s;
}

int tf_platform_system(const char *command, int32_t *code)
{
    const int n = MultiByteToWideChar(CP_UTF8, 0, command, -1, NULL, 0);
    if (n == 0)
    {
        errno = EINVAL;
        return -1;
    }
    wchar_t *w = tf_xmalloc((size_t)n * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, command, -1, w, n);
    const int status = _wsystem(w);
    free(w);
    if (status == -1)
    {
        return -1;
    }
    *code = status;
    return 0;
}

void tf_platform_clock(int64_t *seconds, int32_t *microseconds)
{
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    /* 100-nanosecond ticks since 1601 to microseconds since 1970. */
    const uint64_t ticks = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    const uint64_t micros = ticks / 10 - UINT64_C(11644473600000000);
    *seconds = (int64_t)(micros / 1000000);
    *microseconds = (int32_t)(micros % 1000000);
}

/* Command$: the arguments after the program's name, split by the rules
   Windows programs use (CommandLineToArgvW's: quotes group, 2n backslashes
   before a quote are n and the quote counts, 2n+1 are n and a literal
   quote) and joined by single spaces, so the text is the same as on
   Linux and macOS, where the shell has already done this. */
char *tf_platform_command(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const wchar_t *p = GetCommandLineW();
    /* The program's name: quoted or not, with no escapes. */
    if (*p == L'"')
    {
        for (++p; *p != L'\0' && *p != L'"'; ++p)
        {
        }
        if (*p == L'"')
        {
            ++p;
        }
    }
    else
    {
        while (*p != L'\0' && *p != L' ' && *p != L'\t')
        {
            ++p;
        }
    }
    wchar_t *out = tf_xmalloc((wcslen(p) + 1) * sizeof(wchar_t));
    size_t n = 0;
    int first = 1;
    for (;;)
    {
        while (*p == L' ' || *p == L'\t')
        {
            ++p;
        }
        if (*p == L'\0')
        {
            break;
        }
        if (!first)
        {
            out[n++] = L' ';
        }
        first = 0;
        int quoted = 0;
        while (*p != L'\0' && (quoted || (*p != L' ' && *p != L'\t')))
        {
            size_t slashes = 0;
            while (*p == L'\\')
            {
                ++slashes;
                ++p;
            }
            if (*p == L'"')
            {
                for (size_t i = 0; i < slashes / 2; ++i)
                {
                    out[n++] = L'\\';
                }
                if (slashes % 2 == 1)
                {
                    out[n++] = L'"';
                }
                else if (quoted && p[1] == L'"')
                {
                    out[n++] = L'"'; /* "" inside quotes: a quote */
                    ++p;
                }
                else
                {
                    quoted = !quoted;
                }
                ++p;
            }
            else
            {
                for (size_t i = 0; i < slashes; ++i)
                {
                    out[n++] = L'\\';
                }
                if (*p != L'\0' && (quoted || (*p != L' ' && *p != L'\t')))
                {
                    out[n++] = *p++;
                }
            }
        }
    }
    out[n] = L'\0';
    char *tail = narrow(out);
    free(out);
    return tail;
}

#else
/* ISO C forbids an empty translation unit. */
typedef int tf_sys_win32_not_used;
#endif
