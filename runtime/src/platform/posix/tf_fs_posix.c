/* SPDX-License-Identifier: 0BSD */
/*
 * tf_fs_posix.c -- the file system on POSIX: paths are UTF-8 already, and
 * off_t gives 64-bit positions (with _FILE_OFFSET_BITS on 32-bit Linux).
 */
#if !defined(_WIN32)

#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L

#include "../../tf_internal.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

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

int tf_platform_remove(const char *path)
{
    return unlink(path);
}

int tf_platform_mkdir(const char *path)
{
    return mkdir(path, 0777); /* less the umask, as any program's */
}

int tf_platform_rmdir(const char *path)
{
    return rmdir(path);
}

int tf_platform_chdir(const char *path)
{
    return chdir(path);
}

int tf_platform_rename(const char *from, const char *to)
{
    /* rename() would replace to; VB's Name does not. (A race with another
       program making `to` meanwhile is accepted.) */
    struct stat st;
    if (lstat(to, &st) == 0)
    {
        errno = EEXIST;
        return -1;
    }
    return rename(from, to);
}

int tf_platform_stat(const char *path, int64_t *size, int64_t *modified, int *folder)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
        return -1;
    }
    *size = (int64_t)st.st_size;
    *modified = (int64_t)st.st_mtime;
    *folder = S_ISDIR(st.st_mode) ? 1 : 0;
    return 0;
}

char *tf_platform_getcwd(void)
{
    for (size_t size = 256;; size *= 2)
    {
        char *buffer = tf_xmalloc(size);
        if (getcwd(buffer, size) != NULL)
        {
            return buffer;
        }
        free(buffer);
        if (errno != ERANGE)
        {
            return NULL;
        }
    }
}

int tf_platform_list(const char *dir, void (*each)(const char *name, int folder, void *context),
                     void *context)
{
    const char *base = dir[0] != '\0' ? dir : "."; /* "" is the current folder */
    DIR *d = opendir(base);
    if (d == NULL)
    {
        return -1;
    }
    const size_t baseLength = strlen(base);
    for (struct dirent *e = readdir(d); e != NULL; e = readdir(d))
    {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
        {
            continue;
        }
        /* d_type is not always filled in, so ask the file system; lstat
           first, so a link to a folder can be told from a folder. */
        char *full = tf_xmalloc(baseLength + strlen(e->d_name) + 2);
        strcpy(full, base);
        strcat(full, "/");
        strcat(full, e->d_name);
        struct stat st;
        int folder = TF_ENTRY_FILE;
        if (lstat(full, &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                folder = TF_ENTRY_FOLDER;
            }
            else if (S_ISLNK(st.st_mode) && stat(full, &st) == 0 && S_ISDIR(st.st_mode))
            {
                folder = TF_ENTRY_LINKED_FOLDER;
            }
        }
        free(full);
        each(e->d_name, folder, context);
    }
    closedir(d);
    return 0;
}

int tf_platform_is_link(const char *path)
{
    struct stat st;
    return lstat(path, &st) == 0 && S_ISLNK(st.st_mode);
}

void tf_platform_local_time(int64_t seconds, int32_t fields[6])
{
    const time_t t = (time_t)seconds;
    struct tm tm;
    localtime_r(&t, &tm);
    fields[0] = tm.tm_year + 1900;
    fields[1] = tm.tm_mon + 1;
    fields[2] = tm.tm_mday;
    fields[3] = tm.tm_hour;
    fields[4] = tm.tm_min;
    fields[5] = tm.tm_sec;
}

#else
/* ISO C forbids an empty translation unit. */
typedef int tf_fs_posix_not_used;
#endif
