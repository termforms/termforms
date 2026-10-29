/* SPDX-License-Identifier: 0BSD */
/*
 * tf_sys_posix.c -- the system on POSIX: commands through /bin/sh, the
 * real-time clock, argv for Command$ and CommandArgs(), and the
 * executable's path for App.
 */
#if !defined(_WIN32)

#define _POSIX_C_SOURCE 200809L

#include "../../tf_internal.h"

#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>
#endif

int tf_platform_system(const char *command, int32_t *code)
{
    const int status = system(command);
    if (status == -1)
    {
        return -1;
    }
    /* A command ended by a signal reports 128 + the signal, as sh does. */
    *code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    return 0;
}

void tf_platform_clock(int64_t *seconds, int32_t *microseconds)
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    *seconds = (int64_t)now.tv_sec;
    *microseconds = (int32_t)(now.tv_nsec / 1000);
}

int64_t tf_platform_monotonic_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

/* The shell has already split the command line, so argv is the answer. */
char **tf_platform_args(int argc, char **argv, int32_t *count)
{
    *count = argc > 1 ? argc - 1 : 0;
    char **args = tf_xmalloc(sizeof(char *) * (size_t)(*count > 0 ? *count : 1));
    for (int32_t i = 0; i < *count; ++i)
    {
        const size_t n = strlen(argv[i + 1]) + 1;
        args[i] = tf_xmalloc(n);
        memcpy(args[i], argv[i + 1], n);
    }
    return args;
}

/* From the system rather than argv[0], which is only what the caller
   chose to pass (a bare name found on PATH, or anything at all). */
char *tf_platform_exe_path(void)
{
#if defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(NULL, &size);
    char *found = tf_xmalloc(size + 1);
    if (_NSGetExecutablePath(found, &size) != 0)
    {
        free(found);
        return NULL;
    }
    char *real = realpath(found, NULL);
    if (real == NULL)
    {
        return found;
    }
    free(found);
    char *copy = tf_xmalloc(strlen(real) + 1);
    strcpy(copy, real);
    free(real); /* realpath's own malloc, not tf_xmalloc */
    return copy;
#else
    /* Linux: the kernel's link is already the real file. */
    size_t size = 256;
    for (;;)
    {
        char *path = tf_xmalloc(size);
        const ssize_t n = readlink("/proc/self/exe", path, size);
        if (n < 0)
        {
            free(path);
            return NULL;
        }
        if ((size_t)n < size)
        {
            path[n] = '\0';
            return path;
        }
        free(path);
        size *= 2;
    }
#endif
}

#else
/* ISO C forbids an empty translation unit. */
typedef int tf_sys_posix_not_used;
#endif
