/* SPDX-License-Identifier: 0BSD */
/*
 * tf_error.c -- program start-up, termination, runtime errors and checked
 * memory allocation.
 */
#include "../tf_internal.h"

#include <stdio.h>
#include <stdlib.h>

int32_t tf_current_line = 0;

static const char *source_file = "program";
static void (*shutdown_hook)(void) = NULL;

void tf_runtime_init(const char *main_source_file)
{
    if (main_source_file != NULL)
    {
        source_file = main_source_file;
    }
}

void tf_set_shutdown_hook(void (*hook)(void))
{
    shutdown_hook = hook;
}

void tf_run_shutdown_hook(void)
{
    /* Clear first: the hook must run once even if it triggers an error. */
    void (*hook)(void) = shutdown_hook;
    shutdown_hook = NULL;
    if (hook != NULL)
    {
        hook();
    }
}

void tf_end(int exit_code)
{
    tf_run_shutdown_hook();
    fflush(stdout);
    exit(exit_code);
}

void tf_runtime_error(int number, const char *message)
{
    /* Leave raw mode and the alternate screen first, or the message is lost. */
    tf_run_shutdown_hook();
    fflush(stdout);
    fprintf(stderr, "%s:%ld: runtime error %d: %s\n", source_file, (long)tf_current_line, number,
            message);
    exit(1);
}

void *tf_xmalloc(size_t size)
{
    void *block = malloc(size == 0 ? 1 : size);
    if (block == NULL)
    {
        tf_runtime_error(TF_ERR_OUT_OF_MEMORY, "Out of memory");
    }
    return block;
}

void *tf_xcalloc(size_t count, size_t size)
{
    void *block = calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
    if (block == NULL)
    {
        tf_runtime_error(TF_ERR_OUT_OF_MEMORY, "Out of memory");
    }
    return block;
}

void *tf_xrealloc(void *block, size_t size)
{
    void *grown = realloc(block, size == 0 ? 1 : size);
    if (grown == NULL)
    {
        tf_runtime_error(TF_ERR_OUT_OF_MEMORY, "Out of memory");
    }
    return grown;
}
