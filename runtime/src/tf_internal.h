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

#endif /* TERMFORMS_TF_INTERNAL_H */
