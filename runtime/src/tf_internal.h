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
void tf_run_shutdown_hook(void);

/* ------------------------------------------------------------------------ */
/* Files: the platform's part (platform/<os>/tf_fs_<os>.c)                  */
/* ------------------------------------------------------------------------ */

#include <stdio.h>

/* fopen with a UTF-8 path (Windows: converted to UTF-16 for _wfopen);
   errno is set on failure. */
FILE *tf_platform_fopen(const char *path, const char *mode);
/* 64-bit positions (Windows' long is 32 bits). */
int tf_platform_fseek(FILE *f, int64_t offset, int origin);
int64_t tf_platform_ftell(FILE *f);

/* A zeroed, ready element of the given kind, to hand out after an error
   instead of a real one (see tf_array.c). */
void *tf_scratch_element(int32_t elem, const tf_record_ops *ops);

#endif /* TERMFORMS_TF_INTERNAL_H */
