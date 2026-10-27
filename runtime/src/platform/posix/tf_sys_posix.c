/* SPDX-License-Identifier: 0BSD */
/*
 * tf_sys_posix.c -- the system on POSIX: commands through /bin/sh, the
 * real-time clock, and argv joined for Command$.
 */
#if !defined(_WIN32)

#define _POSIX_C_SOURCE 200809L

#include "../../tf_internal.h"

#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

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

char *tf_platform_command(int argc, char **argv)
{
    size_t length = 1;
    for (int i = 1; i < argc; ++i)
    {
        length += strlen(argv[i]) + 1;
    }
    char *tail = tf_xmalloc(length);
    tail[0] = '\0';
    for (int i = 1; i < argc; ++i)
    {
        if (i > 1)
        {
            strcat(tail, " ");
        }
        strcat(tail, argv[i]);
    }
    return tail;
}

#else
/* ISO C forbids an empty translation unit. */
typedef int tf_sys_posix_not_used;
#endif
