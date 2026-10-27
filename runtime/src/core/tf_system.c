/* SPDX-License-Identifier: 0BSD */
/*
 * tf_system.c -- Command$, Environ$, Shell and the clock (see "The system"
 * in tf_runtime.h); the operating system's part is tf_sys_<os>.c.
 */
#include "../tf_internal.h"

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

tf_string *tf_command(void)
{
    char *tail = tf_platform_command(saved_argc, saved_argv);
    tf_string *s = text(tail);
    free(tail);
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
