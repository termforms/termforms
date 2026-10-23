/* SPDX-License-Identifier: 0BSD */
/*
 * tf_runtime.h -- public interface of the TermForms runtime library (tfrt).
 *
 * Every program produced by the TermForms compiler includes this header and
 * links against the runtime. The interface is plain C99 so that generated
 * code, the runtime and any C library a program declares can all be compiled
 * by the same C compiler.
 *
 * Naming: every public identifier starts with tf_ (functions, types) or TF_
 * (macros), so generated code and user C libraries cannot collide with it.
 *
 * Ownership convention for strings (tf_string *):
 *   - NULL is a valid string and means "" everywhere.
 *   - A function documented as returning "+1" hands the caller one reference,
 *     which the caller must eventually pass to tf_str_release() or store with
 *     tf_str_assign().
 *   - Arguments are borrowed: functions never release what they are given.
 */
#ifndef TERMFORMS_TF_RUNTIME_H
#define TERMFORMS_TF_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Marks functions that never return, so compilers can check callers. */
#if defined(_MSC_VER)
#define TF_NORETURN __declspec(noreturn)
#elif defined(__GNUC__) || defined(__clang__)
#define TF_NORETURN __attribute__((noreturn))
#else
#define TF_NORETURN
#endif

/* ------------------------------------------------------------------------ */
/* Version and lifetime                                                     */
/* ------------------------------------------------------------------------ */

/* Returns the runtime's version as a static "major.minor.patch" string. */
const char *tf_runtime_version(void);

/*
 * Called first thing in a compiled program's main(). Records the program's
 * main source file name for runtime error messages.
 */
void tf_runtime_init(const char *source_file);

/* Restores the terminal (if a form was shown) and exits with the given code. */
TF_NORETURN void tf_end(int exit_code);

/* ------------------------------------------------------------------------ */
/* Runtime errors                                                           */
/* ------------------------------------------------------------------------ */

/* Error numbers follow the VB convention where one exists. */
#define TF_ERR_OVERFLOW 6
#define TF_ERR_OUT_OF_MEMORY 7
#define TF_ERR_DIVISION_BY_ZERO 11
#define TF_ERR_INVALID_ARGUMENT 5
#define TF_ERR_TERMINAL 1001

/*
 * The BASIC source line currently executing. Generated code updates it with
 * TF_LINE() before each statement so runtime errors can name the line.
 */
extern int32_t tf_current_line;
#define TF_LINE(n) (tf_current_line = (n))

/*
 * Reports a runtime error and terminates the program: restores the terminal,
 * prints "file:line: runtime error N: message" to stderr and exits with
 * status 1. (Try/Catch replaces this path in a later phase.)
 */
TF_NORETURN void tf_runtime_error(int number, const char *message);

/* ------------------------------------------------------------------------ */
/* Strings: immutable, reference-counted UTF-8                              */
/* ------------------------------------------------------------------------ */

typedef struct tf_string tf_string;

/* Creates a string from UTF-8 bytes (not necessarily NUL-terminated). +1 */
tf_string *tf_str_from_utf8(const char *bytes, int32_t length);

/*
 * Returns the string for a literal, creating it on first use and caching it
 * in *cache. Literal strings are never freed. The result is borrowed.
 */
tf_string *tf_str_literal(tf_string **cache, const char *bytes, int32_t length);

void tf_str_retain(tf_string *s);
void tf_str_release(tf_string *s);

/* Takes a reference to a borrowed string and returns it. +1 */
tf_string *tf_str_ref(tf_string *s);

/* Stores an owned (+1) value into a variable, releasing the previous value. */
void tf_str_assign(tf_string **slot, tf_string *owned);

/* a & b. +1 */
tf_string *tf_str_concat(const tf_string *a, const tf_string *b);

/* Decimal text of an Integer, as & and Str$ produce it. +1 */
tf_string *tf_str_from_i32(int32_t value);

/* Byte-wise comparison: negative, zero or positive, like strcmp. */
int tf_str_compare(const tf_string *a, const tf_string *b);

/* Len: number of code points. */
int32_t tf_str_len(const tf_string *s);

/* LenB: number of bytes. */
int32_t tf_str_lenb(const tf_string *s);

/* The string's bytes, NUL-terminated; "" for NULL. Valid while s is alive. */
const char *tf_str_cstr(const tf_string *s);

/* ------------------------------------------------------------------------ */
/* Integer (32-bit) arithmetic with overflow checking                       */
/* ------------------------------------------------------------------------ */

int32_t tf_i32_add(int32_t a, int32_t b);
int32_t tf_i32_sub(int32_t a, int32_t b);
int32_t tf_i32_mul(int32_t a, int32_t b);
int32_t tf_i32_div(int32_t a, int32_t b); /* integer division (\), truncates */
int32_t tf_i32_neg(int32_t a);

/* ------------------------------------------------------------------------ */
/* Forms and controls                                                       */
/* ------------------------------------------------------------------------ */

typedef struct tf_control tf_control;

typedef enum tf_control_type
{
    TF_CTL_FORM,
    TF_CTL_TEXTBOX,
    TF_CTL_COMMANDBUTTON
} tf_control_type;

/* Properties a control may have; the compiler checks which apply to which. */
typedef enum tf_prop
{
    TF_PROP_CAPTION,
    TF_PROP_TEXT,
    TF_PROP_LEFT,
    TF_PROP_TOP,
    TF_PROP_WIDTH,
    TF_PROP_HEIGHT
} tf_prop;

typedef enum tf_event
{
    TF_EVENT_CLICK,
    TF_EVENT_CHANGE,
    TF_EVENT_COUNT
} tf_event;

typedef void (*tf_handler)(void);

/* Creates a form (borrowed name). The form owns its controls. */
tf_control *tf_form_new(const char *name);

/* Creates a control on a form; tab order follows creation order. */
tf_control *tf_control_new(tf_control *form, tf_control_type type, const char *name);

void tf_control_on(tf_control *control, tf_event event, tf_handler handler);

/* Property access. Setters borrow the value; tf_get_str returns borrowed. */
void tf_set_str(tf_control *control, tf_prop prop, const tf_string *value);
tf_string *tf_get_str(const tf_control *control, tf_prop prop);
void tf_set_int(tf_control *control, tf_prop prop, int32_t value);
int32_t tf_get_int(const tf_control *control, tf_prop prop);

/*
 * Shows the form and runs its event loop until the user closes it
 * (Ctrl+Q in this version) or the program executes End. Returns the exit code.
 */
int tf_form_run(tf_control *form);

#ifdef __cplusplus
}
#endif

#endif /* TERMFORMS_TF_RUNTIME_H */
