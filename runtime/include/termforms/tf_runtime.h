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

/* Immutable, reference-counted UTF-8 string; see "Strings" below. */
typedef struct tf_string tf_string;

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
#define TF_ERR_TYPE_MISMATCH 13
#define TF_ERR_TERMINAL 1001

/*
 * The BASIC source line currently executing. Generated code updates it with
 * TF_LINE() before each statement so runtime errors can name the line.
 */
extern int32_t tf_current_line;
#define TF_LINE(n) (tf_current_line = (n))

/*
 * An error the program cannot handle (out of memory, the terminal lost):
 * restores the terminal, prints "file:line: runtime error N: message" to
 * stderr and exits with status 1, even inside a Try.
 */
TF_NORETURN void tf_fatal_error(int number, const char *message);

/*
 * Errors a Try can catch. tf_raise records a runtime error (message is
 * borrowed); tf_throw is Throw and Err.Raise (a NULL description gives VB's
 * standard text for the number). Each sets Err and tf_error_pending and
 * returns, so the caller must return a harmless value; generated code tests
 * tf_error_pending after the statement. With no Try active, nothing could
 * catch the error, and both report it and exit as tf_fatal_error does. While
 * an error is pending, further errors are ignored: the first is reported.
 */
extern int tf_error_pending;
void tf_raise(int number, const char *message);
void tf_throw(int32_t number, const tf_string *source, const tf_string *description);
void tf_rethrow(void); /* a bare Throw in a Catch: raise Err again */

/* Try blocks: entered at Try, left on the way into Finally (or End Try). */
void tf_try_enter(void);
void tf_try_leave(void);

/* Entering a Catch: the error is handled, Err still describes it. */
void tf_err_catch(void);

/* Err.Number, Err.Line, Err.Description, Err.Source (borrowed), Err.Clear. */
int32_t tf_err_number(void);
int32_t tf_err_line(void);
tf_string *tf_err_description(void);
tf_string *tf_err_source(void);
void tf_err_clear(void);

/*
 * Finally runs with no error pending. tf_err_save sets aside a pending
 * error (and Err) before it; tf_err_restore makes it pending again after
 * (or ends the program if no Try is left to catch it); tf_err_discard drops
 * it when the Finally block raises an error of its own, which replaces it.
 */
typedef struct tf_saved_error
{
    int pending;
    int32_t number;
    int32_t line;
    tf_string *description;
    tf_string *source;
} tf_saved_error;

void tf_err_save(tf_saved_error *saved);
void tf_err_restore(tf_saved_error *saved);
void tf_err_discard(tf_saved_error *saved);

/* After an event handler or Form_Load: an error nobody caught ends the
   program with its report. */
void tf_check_uncaught(void);

/* ------------------------------------------------------------------------ */
/* Strings: immutable, reference-counted UTF-8                              */
/* ------------------------------------------------------------------------ */

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
/* Integer (32-bit) and Long (64-bit) arithmetic with overflow checking     */
/* ------------------------------------------------------------------------ */

int32_t tf_i32_add(int32_t a, int32_t b);
int32_t tf_i32_sub(int32_t a, int32_t b);
int32_t tf_i32_mul(int32_t a, int32_t b);
int32_t tf_i32_div(int32_t a, int32_t b); /* integer division (\), truncates */
int32_t tf_i32_mod(int32_t a, int32_t b); /* Mod: the sign follows a, as in VB */
int32_t tf_i32_neg(int32_t a);

int64_t tf_i64_add(int64_t a, int64_t b);
int64_t tf_i64_sub(int64_t a, int64_t b);
int64_t tf_i64_mul(int64_t a, int64_t b);
int64_t tf_i64_div(int64_t a, int64_t b);
int64_t tf_i64_mod(int64_t a, int64_t b);
int64_t tf_i64_neg(int64_t a);

/* ------------------------------------------------------------------------ */
/* Double arithmetic                                                        */
/* ------------------------------------------------------------------------ */

/*
 * Doubles never hold Infinity or NaN: a result too large to represent is
 * error 6 (Overflow), division by zero is error 11, and an undefined power
 * such as (-8) ^ 0.5 is error 5. Negation cannot fail and is plain C.
 */
double tf_f64_add(double a, double b);
double tf_f64_sub(double a, double b);
double tf_f64_mul(double a, double b);
double tf_f64_div(double a, double b); /* /, for every numeric type */
double tf_f64_mod(double a, double b);
double tf_f64_pow(double a, double b); /* ^, for every numeric type */

/* ------------------------------------------------------------------------ */
/* Conversions                                                              */
/* ------------------------------------------------------------------------ */

/*
 * CInt and CLng. Fractions round half to even, as in VB (2.5 -> 2,
 * 3.5 -> 4); a value out of range is error 6 (Overflow). From a String the
 * whole text must be a number, else error 13 (Type mismatch).
 */
int32_t tf_i32_from_i64(int64_t value);
int32_t tf_i32_from_f64(double value);
int32_t tf_i32_from_str(const tf_string *text);
int64_t tf_i64_from_f64(double value);
int64_t tf_i64_from_str(const tf_string *text);

/* CDbl from a String: as CInt, without rounding. */
double tf_f64_from_str(const tf_string *text);

/* Fix (towards zero) and Int (towards minus infinity) of a Double. */
double tf_f64_fix(double value);
double tf_f64_int(double value);

/*
 * Val: reads a number from the start of the text, ignoring spaces, and
 * stops at the first character that cannot continue it; &H and &O prefixes
 * read hexadecimal and octal. Text with no number gives 0. Never an error,
 * except Overflow for a number too large for a Double.
 */
double tf_val(const tf_string *text);

/*
 * Text of numbers, as & and CStr produce it (+1). A Double prints as the
 * shortest text that reads back as the same value: "0.1", "3", "1E+20",
 * "1.5E-07" (exponent form from 1E15 up and below 0.0001, as VB.NET).
 */
tf_string *tf_str_from_i64(int64_t value);
tf_string *tf_str_from_f64(double value);

/* Str$: the same text with a leading space when value >= 0, as in VB. +1 */
tf_string *tf_str_str_i64(int64_t value);
tf_string *tf_str_str_f64(double value);

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
