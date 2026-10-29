/* SPDX-License-Identifier: 0BSD */
/*
 * tf_system.c -- Command$, CommandArgs(), App, Environ$, Shell and the
 * clock (see "The system" in tf_runtime.h); the operating system's part is
 * tf_sys_<os>.c.
 */
#include "../tf_internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int saved_argc = 0;
static char **saved_argv = NULL;
static void (*suspend_hook)(void) = NULL;
static void (*resume_hook)(void) = NULL;

void tf_runtime_args(int argc, char **argv)
{
    saved_argc = argc;
    saved_argv = argv;
}

void tf_set_terminal_hooks(void (*suspend)(void), void (*resume)(void))
{
    suspend_hook = suspend;
    resume_hook = resume;
}

static tf_string *text(const char *s)
{
    return tf_str_from_utf8(s, (int32_t)strlen(s));
}

static void free_args(char **args, int32_t count)
{
    for (int32_t i = 0; i < count; ++i)
    {
        free(args[i]);
    }
    free(args);
}

tf_string *tf_command(void)
{
    int32_t count = 0;
    char **args = tf_platform_args(saved_argc, saved_argv, &count);
    size_t length = 1;
    for (int32_t i = 0; i < count; ++i)
    {
        length += strlen(args[i]) + 1;
    }
    char *tail = tf_xmalloc(length);
    tail[0] = '\0';
    for (int32_t i = 0; i < count; ++i)
    {
        if (i > 0)
        {
            strcat(tail, " ");
        }
        strcat(tail, args[i]);
    }
    free_args(args, count);
    tf_string *s = text(tail);
    free(tail);
    return s;
}

void tf_command_args(tf_array *to)
{
    if (to->locks > 0 || to->fixed)
    {
        tf_raise(TF_ERR_ARRAY_LOCKED, NULL);
        return;
    }
    int32_t count = 0;
    char **args = tf_platform_args(saved_argc, saved_argv, &count);
    const int32_t upper = count - 1;
    tf_array_redim(to, TF_ELEM_STRING, NULL, 0, 1, &upper);
    for (int32_t i = 0; i < count; ++i)
    {
        *(tf_string **)tf_array_at1(to, i) = text(args[i]);
    }
    free_args(args, count);
}

/* Where the folder ends in a path: its last separator (either kind on
   Windows), or -1. */
static ptrdiff_t last_separator(const char *path)
{
    ptrdiff_t at = -1;
    for (const char *p = path; *p != '\0'; ++p)
    {
#if defined(_WIN32)
        if (*p == '/' || *p == '\\')
#else
        if (*p == '/')
#endif
        {
            at = p - path;
        }
    }
    return at;
}

tf_string *tf_app_fullpath(void)
{
    char *path = tf_platform_exe_path();
    if (path == NULL)
    {
        return NULL;
    }
    tf_string *s = text(path);
    free(path);
    return s;
}

/* The root keeps its separator ("/", or "C:\" on Windows), as VB6's App.Path does. */
tf_string *tf_app_path(void)
{
    char *path = tf_platform_exe_path();
    if (path == NULL)
    {
        return NULL;
    }
    ptrdiff_t end = last_separator(path);
    if (end == 0 || (end == 2 && path[1] == ':'))
    {
        ++end;
    }
    tf_string *s = tf_str_from_utf8(path, (int32_t)(end > 0 ? end : 0));
    free(path);
    return s;
}

tf_string *tf_app_exename(void)
{
    char *path = tf_platform_exe_path();
    if (path == NULL)
    {
        return NULL;
    }
    const char *name = path + last_separator(path) + 1;
    size_t length = strlen(name);
#if defined(_WIN32)
    if (length > 4 && _stricmp(name + length - 4, ".exe") == 0)
    {
        length -= 4;
    }
#endif
    tf_string *s = tf_str_from_utf8(name, (int32_t)length);
    free(path);
    return s;
}

tf_string *tf_environ(const tf_string *name)
{
    const char *value = tf_str_lenb(name) > 0 ? tf_platform_getenv(tf_str_cstr(name)) : NULL;
    return value != NULL ? text(value) : NULL;
}

int32_t tf_shell(const tf_string *command)
{
    if (tf_str_lenb(command) == 0)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, NULL);
        return 0;
    }
    if (suspend_hook != NULL)
    {
        suspend_hook();
    }
    fflush(stdout);
    int32_t code = 0;
    const int failed = tf_platform_system(tf_str_cstr(command), &code);
    if (resume_hook != NULL)
    {
        resume_hook();
    }
    if (failed != 0)
    {
        tf_raise(TF_ERR_FILE_NOT_FOUND, NULL); /* as VB's Shell: it could not be started */
        return 0;
    }
    return code;
}

/* The local time now: year, month, day, hour, minute, second; *micro the
   microseconds past the second. */
static void local_now(int32_t f[6], int32_t *micro)
{
    int64_t seconds = 0;
    tf_platform_clock(&seconds, micro);
    tf_platform_local_time(seconds, f);
}

tf_string *tf_now(void)
{
    int32_t f[6];
    int32_t micro = 0;
    local_now(f, &micro);
    char buffer[32];
    snprintf(buffer, sizeof buffer, "%04d-%02d-%02d %02d:%02d:%02d", (int)f[0], (int)f[1],
             (int)f[2], (int)f[3], (int)f[4], (int)f[5]);
    return text(buffer);
}

tf_string *tf_date(void)
{
    int32_t f[6];
    int32_t micro = 0;
    local_now(f, &micro);
    char buffer[32];
    snprintf(buffer, sizeof buffer, "%02d-%02d-%04d", (int)f[1], (int)f[2], (int)f[0]);
    return text(buffer);
}

tf_string *tf_time(void)
{
    int32_t f[6];
    int32_t micro = 0;
    local_now(f, &micro);
    char buffer[32];
    snprintf(buffer, sizeof buffer, "%02d:%02d:%02d", (int)f[3], (int)f[4], (int)f[5]);
    return text(buffer);
}

double tf_timer(void)
{
    int32_t f[6];
    int32_t micro = 0;
    local_now(f, &micro);
    return f[3] * 3600.0 + f[4] * 60.0 + f[5] + micro / 1e6;
}
