/* SPDX-License-Identifier: 0BSD */
/*
 * tf_sys_win32.c -- the system on Windows: commands through cmd.exe as
 * UTF-16 (_wsystem), the precise system clock, the command line's
 * arguments for Command$ and CommandArgs(), the executable's path, and
 * standard input and output for console programs.
 */
#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include "../../tf_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int64_t tf_platform_monotonic_ms(void)
{
    return (int64_t)GetTickCount64();
}

/* The arguments after the program's name, split by the rules Windows
   programs use (CommandLineToArgvW's: quotes group, 2n backslashes before
   a quote are n and the quote counts, 2n+1 are n and a literal quote), so
   they are what the shell gives on Linux and macOS. Read from the UTF-16
   command line, as argv holds them in the ANSI code page. */
char **tf_platform_args(int argc, char **argv, int32_t *count)
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
    /* Each argument is written into out ending with a NUL; starts[i] is
       where argument i begins. There are at most half as many arguments
       as characters left, plus one. */
    const size_t left = wcslen(p);
    wchar_t *out = tf_xmalloc((left + 1) * 2 * sizeof(wchar_t));
    size_t *starts = tf_xmalloc((left / 2 + 1) * sizeof(size_t));
    size_t n = 0;
    int32_t found = 0;
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
        starts[found++] = n;
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
        out[n++] = L'\0';
    }
    char **args = tf_xmalloc(sizeof(char *) * (size_t)(found > 0 ? found : 1));
    for (int32_t i = 0; i < found; ++i)
    {
        args[i] = narrow(out + starts[i]);
    }
    free(starts);
    free(out);
    *count = found;
    return args;
}

/* A console takes text in its own code page through WriteFile, so to one
   the UTF-8 goes as UTF-16 with WriteConsoleW; to a pipe or a file it goes
   as it is (UTF-8, LF line ends, as on Linux and macOS). */
void tf_platform_console_write(int stream, const char *data, size_t n)
{
    fflush(stdout); /* stdio's own text first (a headless dump) */
    HANDLE h = GetStdHandle(stream == 2 ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    if (h == NULL || h == INVALID_HANDLE_VALUE || n == 0)
    {
        return;
    }
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode))
    {
        const int wn = MultiByteToWideChar(CP_UTF8, 0, data, (int)n, NULL, 0);
        if (wn <= 0)
        {
            return;
        }
        wchar_t *w = tf_xmalloc((size_t)wn * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, 0, data, (int)n, w, wn);
        DWORD written = 0;
        WriteConsoleW(h, w, (DWORD)wn, &written, NULL);
        free(w);
        return;
    }
    DWORD written = 0;
    WriteFile(h, data, (DWORD)n, &written, NULL);
}

/* From a console, ReadConsoleW gives what was typed as UTF-16, a line at a
   time (Ctrl+Z at the start of a line is the end of the input, as cmd.exe
   has it); from a pipe or a file, the bytes are read as they are. */
int tf_platform_console_read_line(char **line, size_t *size)
{
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (h != NULL && h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode))
    {
        size_t capacity = 128;
        size_t used = 0;
        wchar_t *w = tf_xmalloc(capacity * sizeof(wchar_t));
        for (;;)
        {
            wchar_t c = 0;
            DWORD got = 0;
            if (!ReadConsoleW(h, &c, 1, &got, NULL) || got == 0)
            {
                break;
            }
            if (c == L'\n')
            {
                break;
            }
            if (c == 0x1A && used == 0)
            {
                free(w);
                return -1;
            }
            if (used + 1 >= capacity)
            {
                capacity *= 2;
                w = tf_xrealloc(w, capacity * sizeof(wchar_t));
            }
            w[used++] = c;
        }
        if (used > 0 && w[used - 1] == L'\r')
        {
            --used;
        }
        w[used] = L'\0';
        *line = narrow(w);
        *size = strlen(*line);
        free(w);
        return 0;
    }
    /* Binary, so stdio neither turns CR LF into LF (done here) nor stops
       at a Ctrl+Z inside the data. */
    static int binary = 0;
    if (!binary)
    {
        _setmode(_fileno(stdin), _O_BINARY);
        binary = 1;
    }
    size_t capacity = 128;
    size_t used = 0;
    char *s = tf_xmalloc(capacity);
    int c = getchar();
    if (c == EOF)
    {
        free(s);
        return -1;
    }
    for (; c != EOF && c != '\n'; c = getchar())
    {
        if (used == capacity)
        {
            capacity *= 2;
            s = tf_xrealloc(s, capacity);
        }
        s[used++] = (char)c;
    }
    if (used > 0 && s[used - 1] == '\r')
    {
        --used;
    }
    *line = s;
    *size = used;
    return 0;
}

/* GetModuleFileNameW does not say how long the path is, only that it was
   cut short, so the buffer grows until it fits. */
char *tf_platform_exe_path(void)
{
    DWORD size = MAX_PATH;
    for (;;)
    {
        wchar_t *path = tf_xmalloc((size_t)size * sizeof(wchar_t));
        const DWORD n = GetModuleFileNameW(NULL, path, size);
        if (n == 0)
        {
            free(path);
            return NULL;
        }
        if (n < size)
        {
            char *s = narrow(path);
            free(path);
            return s;
        }
        free(path);
        size *= 2;
    }
}

#else
/* ISO C forbids an empty translation unit. */
typedef int tf_sys_win32_not_used;
#endif
