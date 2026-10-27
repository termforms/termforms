/* SPDX-License-Identifier: 0BSD */
/*
 * tf_error.c -- program start-up and termination, runtime errors and the
 * Err state behind Try/Catch, and checked memory allocation.
 *
 * Errors are reported by status, not by longjmp (design decision of 23 Oct
 * 2026): a catchable error is recorded here and tf_error_pending is set;
 * generated code tests it after each statement that can fail and jumps to
 * the Try's handler or to its procedure's cleanup, which returns to a
 * caller that tests it in turn. While no Try is active anywhere, nothing
 * could catch an error, so tf_raise() ends the program at once, exactly as
 * before Try existed; programs without Try never see a pending error.
 *
 * Global state, documented as the coding standards require: the program is
 * single-threaded, and there is one error in flight at a time.
 */
#include "../tf_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int32_t tf_current_line = 0;
int tf_error_pending = 0;

static const char *source_file = "program";
static void (*shutdown_hook)(void) = NULL;

/* Try blocks entered and not yet left, across all active procedures. */
static int32_t try_depth = 0;

/* Err: the error in flight, or the last one caught. Strings are owned. */
static int32_t err_number = 0;
static int32_t err_line = 0;
static tf_string *err_description = NULL;
static tf_string *err_source = NULL;

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

/* Leaves raw mode and the alternate screen first, or the message is lost. */
static TF_NORETURN void report_and_exit(int32_t number, int32_t line, const char *message)
{
    tf_run_shutdown_hook();
    fflush(stdout);
    fprintf(stderr, "%s:%ld: runtime error %ld: %s\n", source_file, (long)line, (long)number,
            message);
    exit(1);
}

void tf_fatal_error(int number, const char *message)
{
    report_and_exit(number, tf_current_line, message);
}

/* VB's wording for the errors the runtime raises; others get VB's generic
   text, as Err.Raise does for an application's own numbers. */
static const char *standard_description(int32_t number)
{
    switch (number)
    {
        case TF_ERR_INVALID_ARGUMENT:
            return "Invalid procedure call or argument";
        case TF_ERR_OVERFLOW:
            return "Overflow";
        case TF_ERR_OUT_OF_MEMORY:
            return "Out of memory";
        case TF_ERR_DIVISION_BY_ZERO:
            return "Division by zero";
        case TF_ERR_TYPE_MISMATCH:
            return "Type mismatch";
        default:
            return "Application-defined or object-defined error";
    }
}

/* Replaces Err; takes ownership of description and source. */
static void set_err(int32_t number, tf_string *description, tf_string *source)
{
    tf_str_release(err_description);
    tf_str_release(err_source);
    err_number = number;
    err_line = tf_current_line;
    err_description = description;
    err_source = source;
}

void tf_raise(int number, const char *message)
{
    if (try_depth == 0)
    {
        report_and_exit(number, tf_current_line, message);
    }
    if (tf_error_pending)
    {
        /* The statement goes on to its check after an error, so later steps
           may fail too (dividing by the 0 a failed step returned); the
           first error is the one to report. */
        return;
    }
    set_err(number, tf_str_from_utf8(message, (int32_t)strlen(message)), NULL);
    tf_error_pending = 1;
}

void tf_throw(int32_t number, const tf_string *source, const tf_string *description)
{
    if (number == 0)
    {
        tf_raise(TF_ERR_INVALID_ARGUMENT, standard_description(TF_ERR_INVALID_ARGUMENT));
        return;
    }
    if (tf_error_pending)
    {
        return;
    }
    tf_string *text = description != NULL
                          ? tf_str_ref((tf_string *)description)
                          : tf_str_from_utf8(standard_description(number),
                                             (int32_t)strlen(standard_description(number)));
    set_err(number, text, tf_str_ref((tf_string *)source));
    if (try_depth == 0)
    {
        report_and_exit(err_number, err_line, tf_str_cstr(err_description));
    }
    tf_error_pending = 1;
}

void tf_rethrow(void)
{
    if (try_depth == 0)
    {
        report_and_exit(err_number, err_line, tf_str_cstr(err_description));
    }
    tf_error_pending = 1;
}

void tf_try_enter(void)
{
    ++try_depth;
}

void tf_try_leave(void)
{
    --try_depth;
}

void tf_err_catch(void)
{
    tf_error_pending = 0;
}

int32_t tf_err_number(void)
{
    return err_number;
}

int32_t tf_err_line(void)
{
    return err_line;
}

tf_string *tf_err_description(void)
{
    return err_description;
}

tf_string *tf_err_source(void)
{
    return err_source;
}

void tf_err_clear(void)
{
    set_err(0, NULL, NULL);
    err_line = 0;
}

void tf_err_save(tf_saved_error *saved)
{
    saved->pending = tf_error_pending;
    saved->number = err_number;
    saved->line = err_line;
    saved->description = tf_str_ref(err_description);
    saved->source = tf_str_ref(err_source);
    tf_error_pending = 0;
}

void tf_err_restore(tf_saved_error *saved)
{
    if (!saved->pending)
    {
        tf_err_discard(saved);
        return;
    }
    tf_str_release(err_description);
    tf_str_release(err_source);
    err_number = saved->number;
    err_line = saved->line;
    err_description = saved->description;
    err_source = saved->source;
    saved->description = NULL;
    saved->source = NULL;
    /* With no Try left to run, the error can only end the program. */
    if (try_depth == 0)
    {
        report_and_exit(err_number, err_line, tf_str_cstr(err_description));
    }
    tf_error_pending = 1;
}

void tf_err_discard(tf_saved_error *saved)
{
    tf_str_release(saved->description);
    tf_str_release(saved->source);
    saved->description = NULL;
    saved->source = NULL;
}

void tf_check_uncaught(void)
{
    if (tf_error_pending)
    {
        report_and_exit(err_number, err_line, tf_str_cstr(err_description));
    }
}

void *tf_xmalloc(size_t size)
{
    void *block = malloc(size == 0 ? 1 : size);
    if (block == NULL)
    {
        tf_fatal_error(TF_ERR_OUT_OF_MEMORY, "Out of memory");
    }
    return block;
}

void *tf_xcalloc(size_t count, size_t size)
{
    void *block = calloc(count == 0 ? 1 : count, size == 0 ? 1 : size);
    if (block == NULL)
    {
        tf_fatal_error(TF_ERR_OUT_OF_MEMORY, "Out of memory");
    }
    return block;
}

void *tf_xrealloc(void *block, size_t size)
{
    void *grown = realloc(block, size == 0 ? 1 : size);
    if (grown == NULL)
    {
        tf_fatal_error(TF_ERR_OUT_OF_MEMORY, "Out of memory");
    }
    return grown;
}
