/* SPDX-License-Identifier: 0BSD */
/*
 * tf_internal.h -- declarations shared between runtime source files.
 * Not part of the public interface; generated code never includes it.
 */
#ifndef TERMFORMS_TF_INTERNAL_H
#define TERMFORMS_TF_INTERNAL_H

#include "termforms/tf_runtime.h"

#include <stddef.h>

/* ------------------------------------------------------------------------ */
/* Memory: allocation failure is a runtime error, never a NULL return.      */
/* ------------------------------------------------------------------------ */

void *tf_xmalloc(size_t size);
void *tf_xcalloc(size_t count, size_t size);
void *tf_xrealloc(void *block, size_t size);

/* ------------------------------------------------------------------------ */
/* Strings                                                                  */
/* ------------------------------------------------------------------------ */

/* Layout is internal so that generated code cannot depend on it. */
struct tf_string
{
    int32_t refcount; /* TF_STR_IMMORTAL for literals */
    int32_t length;   /* bytes, excluding the terminating NUL */
    char data[];      /* UTF-8, NUL-terminated */
};

#define TF_STR_IMMORTAL (-1)

/* Largest length representable in the header; longer results are error 14. */
#define TF_STR_MAX_LENGTH (INT32_MAX - 1)

/* A new string of length bytes (1..TF_STR_MAX_LENGTH), refcount 1, its
   bytes for the caller to fill (the terminating NUL is in place). */
tf_string *tf_str_alloc(int32_t length);

/* ------------------------------------------------------------------------ */
/* Shutdown hook                                                            */
/* ------------------------------------------------------------------------ */

/*
 * The forms layer registers a function that restores the terminal. It runs
 * before any error message is printed and before the process exits, so the
 * user's shell is never left in raw mode or on the alternate screen.
 */
void tf_set_shutdown_hook(void (*hook)(void));

/* The main source file's path, as given to tf_runtime_init. */
const char *tf_runtime_source(void);
void tf_run_shutdown_hook(void);

/* ------------------------------------------------------------------------ */
/* Files: the platform's part (platform/<os>/tf_fs_<os>.c)                  */
/* ------------------------------------------------------------------------ */

#include <stdio.h>

#if defined(_WIN32)
#define TF_ON_WINDOWS 1
#else
#define TF_ON_WINDOWS 0
#endif

/* fopen with a UTF-8 path (Windows: converted to UTF-16 for _wfopen);
   errno is set on failure. */
FILE *tf_platform_fopen(const char *path, const char *mode);
/* 64-bit positions (Windows' long is 32 bits). */
int tf_platform_fseek(FILE *f, int64_t offset, int origin);
int64_t tf_platform_ftell(FILE *f);

/* The file system, paths in UTF-8. Each returns 0, or -1 with errno set. */
int tf_platform_remove(const char *path); /* a file */
int tf_platform_mkdir(const char *path);
int tf_platform_rmdir(const char *path);
int tf_platform_chdir(const char *path);
int tf_platform_rename(const char *from, const char *to); /* replaces nothing: EEXIST */
/* size in bytes, modified as seconds since 1970 (UTC), folder: 1 if so. */
int tf_platform_stat(const char *path, int64_t *size, int64_t *modified, int *folder);
/* The current folder, to free. */
char *tf_platform_getcwd(void);
/* Calls each(name, folder, context) for every entry of dir but . and ..;
   -1 with errno if dir cannot be read. */
int tf_platform_list(const char *dir, void (*each)(const char *name, int folder, void *context),
                     void *context);
/* Local time of a time since 1970: year, month (1-12), day, hour, minute,
   second in fields[0..5]. Windows names are matched without case. */
void tf_platform_local_time(int64_t seconds, int32_t fields[6]);
#define TF_NAMES_IGNORE_CASE TF_ON_WINDOWS

/* ------------------------------------------------------------------------ */
/* The system: platform/<os>/tf_sys_<os>.c (getenv: tf_term_<os>.c)         */
/* ------------------------------------------------------------------------ */

/*
 * Reads an environment variable as UTF-8 (on Windows, getenv would convert
 * it to the ANSI code page). Returns NULL if unset. The result stays valid
 * until the next call.
 */
const char *tf_platform_getenv(const char *name);

/* Runs a command through the system's shell (sh, cmd.exe), waiting for it;
   its exit code in *code. -1 with errno if it could not be started. */
int tf_platform_system(const char *command, int32_t *code);

/* The time now: seconds since 1970 (UTC) and the microseconds past them. */
void tf_platform_clock(int64_t *seconds, int32_t *microseconds);

/* Milliseconds from some fixed moment, never going back (for Timers). */
int64_t tf_platform_monotonic_ms(void);

/* Command$: the arguments after the program's name joined by single
   spaces (quotes as the system's rules take them), to free. */
char *tf_platform_command(int argc, char **argv);

/* The forms layer hands the terminal back while Shell runs a command
   (normal mode, main screen), and takes it again after; set when a form
   is shown. */
void tf_set_terminal_hooks(void (*suspend)(void), void (*resume)(void));

/* A zeroed, ready element of the given kind, to hand out after an error
   instead of a real one (see tf_array.c). */
void *tf_scratch_element(int32_t elem, const tf_record_ops *ops);

#endif /* TERMFORMS_TF_INTERNAL_H */
