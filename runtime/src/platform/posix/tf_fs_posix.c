/* SPDX-License-Identifier: 0BSD */
/*
 * tf_fs_posix.c -- the file system on POSIX: paths are UTF-8 already, and
 * off_t gives 64-bit positions (with _FILE_OFFSET_BITS on 32-bit Linux).
 */
#if !defined(_WIN32)

#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L

#include "../../tf_internal.h"

#include <sys/types.h>

FILE *tf_platform_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}

int tf_platform_fseek(FILE *f, int64_t offset, int origin)
{
    return fseeko(f, (off_t)offset, origin);
}

int64_t tf_platform_ftell(FILE *f)
{
    return (int64_t)ftello(f);
}

#else
/* ISO C forbids an empty translation unit. */
typedef int tf_fs_posix_not_used;
#endif
