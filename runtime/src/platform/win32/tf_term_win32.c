/* SPDX-License-Identifier: 0BSD */
/*
 * tf_term_win32.c -- terminal layer for Windows consoles.
 *
 * Works in Windows Terminal and in the console host (conhost) on Windows 10
 * 1809 or later. Both accept VT sequences for output and, with
 * ENABLE_VIRTUAL_TERMINAL_INPUT, deliver special keys as VT sequences too, so
 * everything above this file is shared with POSIX.
 *
 * Input is taken from console input records rather than ReadFile: that lets
 * resize events be seen and keeps the UTF-16 to UTF-8 conversion here, rather
 * than depending on the input code page.
 */
#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include "../../tui/tf_tui.h"

#include <stdlib.h>
#include <string.h>
#include <windows.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef DISABLE_NEWLINE_AUTO_RETURN
#define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif
#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif

static const char ENTER_SEQ[] = "\x1b[?1049h\x1b[?7l\x1b[H\x1b[2J";
static const char LEAVE_SEQ[] = "\x1b[?1002l\x1b[?1006l" /* mouse reports off (if on) */
                                "\x1b[0m\x1b[?25h\x1b[?7h\x1b[?1049l";

static HANDLE input_handle;
static HANDLE output_handle;
static DWORD saved_input_mode;
static DWORD saved_output_mode;
static UINT saved_output_cp;
static int active = 0;
static int resized = 0;
static WCHAR pending_high_surrogate = 0;

/* Restores the console if the window is closed or the user logs off. */
static BOOL WINAPI on_console_event(DWORD event)
{
    if (event == CTRL_CLOSE_EVENT || event == CTRL_LOGOFF_EVENT || event == CTRL_SHUTDOWN_EVENT)
    {
        tf_term_close();
    }
    return FALSE; /* let the default handler end the process */
}

int tf_term_open(const char **reason)
{
    input_handle = GetStdHandle(STD_INPUT_HANDLE);
    output_handle = GetStdHandle(STD_OUTPUT_HANDLE);

    if (!GetConsoleMode(input_handle, &saved_input_mode) ||
        !GetConsoleMode(output_handle, &saved_output_mode))
    {
        *reason = "standard input and output must be a console";
        return -1;
    }

    const DWORD output_mode =
        saved_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    if (!SetConsoleMode(output_handle, output_mode))
    {
        *reason = "this console does not support VT sequences (Windows 10 1809 or later needed)";
        return -1;
    }

    /* No line input, echo or Ctrl+C processing: every key reaches the program. */
    if (!SetConsoleMode(input_handle, ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_WINDOW_INPUT))
    {
        SetConsoleMode(output_handle, saved_output_mode);
        *reason = "this console does not support VT input (Windows 10 1809 or later needed)";
        return -1;
    }

    saved_output_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCtrlHandler(on_console_event, TRUE);
    active = 1;

    tf_term_write(ENTER_SEQ, sizeof ENTER_SEQ - 1);
    return 0;
}

void tf_term_close(void)
{
    if (active)
    {
        active = 0;
        tf_term_write(LEAVE_SEQ, sizeof LEAVE_SEQ - 1);
        SetConsoleMode(input_handle, saved_input_mode);
        SetConsoleMode(output_handle, saved_output_mode);
        SetConsoleOutputCP(saved_output_cp);
        SetConsoleCtrlHandler(on_console_event, FALSE);
    }
}

void tf_term_write(const char *bytes, size_t length)
{
    while (length > 0)
    {
        DWORD written = 0;
        const DWORD chunk = length > 65536 ? 65536 : (DWORD)length;
        if (!WriteFile(output_handle, bytes, chunk, &written, NULL) || written == 0)
        {
            return;
        }
        bytes += written;
        length -= written;
    }
}

/* Appends one code point as UTF-8 if it fits. Returns bytes added. */
static int append_utf8(unsigned char *buffer, int used, int capacity, uint32_t cp)
{
    char encoded[4];
    const int n = tf_utf8_encode(cp, encoded);
    if (used + n > capacity)
    {
        return 0;
    }
    memcpy(buffer + used, encoded, (size_t)n);
    return n;
}

int tf_term_read(unsigned char *buffer, int capacity, int timeout_ms)
{
    const DWORD wait = timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms;
    if (WaitForSingleObject(input_handle, wait) != WAIT_OBJECT_0)
    {
        return 0;
    }

    INPUT_RECORD records[64];
    DWORD count = 0;
    if (!ReadConsoleInputW(input_handle, records, 64, &count))
    {
        tf_fatal_error(TF_ERR_TERMINAL, "Console input failed");
    }

    int used = 0;
    for (DWORD i = 0; i < count; ++i)
    {
        const INPUT_RECORD *r = &records[i];
        if (r->EventType == WINDOW_BUFFER_SIZE_EVENT)
        {
            resized = 1;
            continue;
        }
        if (r->EventType != KEY_EVENT || !r->Event.KeyEvent.bKeyDown)
        {
            continue;
        }

        const WCHAR unit = r->Event.KeyEvent.uChar.UnicodeChar;
        if (unit == 0)
        {
            continue; /* modifier keys alone; special keys arrive as VT text */
        }

        /* Repeat count covers auto-repeat folded into one record. */
        for (WORD rep = 0; rep < r->Event.KeyEvent.wRepeatCount; ++rep)
        {
            if (unit >= 0xD800 && unit <= 0xDBFF)
            {
                pending_high_surrogate = unit;
                continue;
            }
            uint32_t cp = unit;
            if (unit >= 0xDC00 && unit <= 0xDFFF && pending_high_surrogate != 0)
            {
                cp = 0x10000u + (((uint32_t)pending_high_surrogate - 0xD800u) << 10) +
                     ((uint32_t)unit - 0xDC00u);
                pending_high_surrogate = 0;
            }
            used += append_utf8(buffer, used, capacity, cp);
        }
    }
    return used;
}

void tf_term_size(int *cols, int *rows)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(output_handle, &info))
    {
        *cols = info.srWindow.Right - info.srWindow.Left + 1;
        *rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    }
    else
    {
        *cols = 80;
        *rows = 25;
    }
}

int tf_term_take_resize(void)
{
    const int was = resized;
    resized = 0;
    return was;
}

int tf_term_utf8(void)
{
    return 1; /* the output code page is set to UTF-8 in tf_term_open */
}

const char *tf_platform_getenv(const char *name)
{
    static char *value = NULL;
    WCHAR wide_name[256];
    if (MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name, 256) == 0)
    {
        return NULL;
    }

    const DWORD length = GetEnvironmentVariableW(wide_name, NULL, 0);
    if (length == 0)
    {
        return NULL;
    }
    WCHAR *wide = tf_xmalloc((size_t)length * sizeof(WCHAR));
    GetEnvironmentVariableW(wide_name, wide, length);

    const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    free(value);
    value = tf_xmalloc((size_t)bytes);
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, value, bytes, NULL, NULL);
    free(wide);
    return value;
}

#else
/* ISO C forbids an empty translation unit. */
typedef int tf_term_win32_not_used;
#endif
