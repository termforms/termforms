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

/* Marks generated helpers a program may not use (a Type never copied has an
   unused _copy), so strict warnings stay quiet. MSVC warns of these only at
   /W4 (C4505), and has no attribute for it in C. */
#if defined(__GNUC__) || defined(__clang__)
#define TF_MAYBE_UNUSED __attribute__((unused))
#else
#define TF_MAYBE_UNUSED
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

/*
 * Error numbers (policy of 27 Oct 2026, docs/REFERENCE.md "Error numbers"):
 *   1-899     the runtime; VB6's number wherever the meaning is the same,
 *             and never a VB6 number for something different
 *   900-999   the runtime, errors VB6 has no number for
 *   1000 up   programs' own errors (Throw, Err.Raise)
 * tf_error_text gives each number's standard text, VB6's wording where VB6
 * has the number.
 */
#define TF_ERR_INVALID_ARGUMENT 5     /* Invalid procedure call or argument */
#define TF_ERR_OVERFLOW 6             /* Overflow */
#define TF_ERR_OUT_OF_MEMORY 7        /* Out of memory (always fatal) */
#define TF_ERR_SUBSCRIPT 9            /* Subscript out of range */
#define TF_ERR_ARRAY_LOCKED 10        /* This array is fixed or temporarily locked */
#define TF_ERR_DIVISION_BY_ZERO 11    /* Division by zero */
#define TF_ERR_TYPE_MISMATCH 13       /* Type mismatch */
#define TF_ERR_NOTHING 91             /* Object variable or With block variable not set */
#define TF_ERR_KEY_EXISTS 457         /* This key is already associated with an element of ... */
#define TF_ERR_BAD_FILE 52            /* Bad file name or number */
#define TF_ERR_FILE_NOT_FOUND 53      /* File not found */
#define TF_ERR_BAD_FILE_MODE 54       /* Bad file mode */
#define TF_ERR_FILE_OPEN 55           /* File already open */
#define TF_ERR_DEVICE_IO 57           /* Device I/O error */
#define TF_ERR_FILE_EXISTS 58         /* File already exists */
#define TF_ERR_DISK_FULL 61           /* Disk full */
#define TF_ERR_PAST_END 62            /* Input past end of file */
#define TF_ERR_BAD_RECORD 63          /* Bad record number */
#define TF_ERR_TOO_MANY_FILES 67      /* Too many files */
#define TF_ERR_PERMISSION 70          /* Permission denied */
#define TF_ERR_PATH_ACCESS 75         /* Path/File access error */
#define TF_ERR_PATH_NOT_FOUND 76      /* Path not found */
#define TF_ERR_OUT_OF_STRING_SPACE 14 /* Out of string space: a string too long */
#define TF_ERR_TERMINAL 900           /* Terminal error (always fatal) */
#define TF_ERR_FIRST_USER 1000        /* the first number left to programs */

/* The standard text for an error number (static); for a number with no
   meaning of its own, VB's "Application-defined or object-defined error". */
const char *tf_error_text(int32_t number);

/*
 * The BASIC source line currently executing. Generated code updates it with
 * TF_LINE() before each statement so runtime errors can name the line.
 */
extern int32_t tf_current_line;
#define TF_LINE(n) (tf_current_line = (n))

/*
 * An error the program cannot handle (out of memory, the terminal lost):
 * restores the terminal, prints "file:line: runtime error N: message" to
 * stderr and exits with status 1, even inside a Try. A NULL message means
 * the number's standard text.
 */
TF_NORETURN void tf_fatal_error(int number, const char *message);

/*
 * Errors a Try can catch. tf_raise records a runtime error; its message is
 * borrowed, and NULL (the usual case) means the number's standard text, so
 * the wording lives in one table. tf_throw is Throw and Err.Raise (a NULL
 * description likewise gives the standard text). Each sets Err and tf_error_pending and
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

/* a & b. +1. A result longer than a string can hold is error 14. */
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

/* CByte: as CInt, then 0 to 255, else error 6. tf_u8_add is a For loop's
   step over a Byte variable (Step may be negative). */
uint8_t tf_u8_from_i32(int32_t value);
uint8_t tf_u8_from_i64(int64_t value);
uint8_t tf_u8_from_f64(double value);
uint8_t tf_u8_from_str(const tf_string *text);
uint8_t tf_u8_add(int32_t a, int32_t b);

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
/* String functions                                                         */
/* ------------------------------------------------------------------------ */

/*
 * As VB's: positions and lengths count code points and start at 1. The
 * strings passed are borrowed; a String returned is owned. A bad argument
 * (a start before 1, a negative length, ...) is error 5 and returns "" or
 * 0; a result longer than a string can hold is error 14.
 */
#define TF_STR_REST INT32_MAX /* Mid$ with no length: the rest */
tf_string *tf_str_mid(const tf_string *s, int32_t start, int32_t length);
tf_string *tf_str_left(const tf_string *s, int32_t n);
tf_string *tf_str_right(const tf_string *s, int32_t n);
int32_t tf_str_instr(int32_t start, const tf_string *s, const tf_string *find);    /* 0: none */
int32_t tf_str_instrrev(const tf_string *s, const tf_string *find, int32_t start); /* -1: end */
tf_string *tf_str_lcase(const tf_string *s); /* ASCII letters only, for now */
tf_string *tf_str_ucase(const tf_string *s);
tf_string *tf_str_ltrim(const tf_string *s); /* spaces, as VB: not tabs */
tf_string *tf_str_rtrim(const tf_string *s);
tf_string *tf_str_trim(const tf_string *s);
tf_string *tf_str_space(int32_t n);
tf_string *tf_str_repeat(int32_t n, const tf_string *s); /* String$: s's first character */
tf_string *tf_str_chr(int32_t code);                     /* a Unicode code point */
int32_t tf_str_asc(const tf_string *s);                  /* the first code point */
tf_string *tf_str_replace(const tf_string *s, const tf_string *find, const tf_string *with);
tf_string *tf_str_reverse(const tf_string *s);

/* Split and Join, which use arrays, are declared with them below. */

/* ------------------------------------------------------------------------ */
/* Files                                                                    */
/* ------------------------------------------------------------------------ */

/*
 * VB's numbered files: Open path For mode As #n, then statements on #n.
 * Numbers run from 1 to TF_FILE_MAX. Text is UTF-8: a line is read up to
 * LF (a CR before it dropped), bytes that are not UTF-8 become U+FFFD, and
 * Print # writes LF on every platform. Errors are VB6's: 52 for a number
 * out of range or not open, 54 for the wrong mode, 55 for a number in
 * use, 62 for reading past the end, and 53, 70, 75, 76, 61, 67 from the
 * operating system. Files still open when the program ends are closed
 * (C's exit flushes them).
 */
#define TF_FILE_MAX 255
#define TF_FILE_INPUT 1
#define TF_FILE_OUTPUT 2
#define TF_FILE_APPEND 3
#define TF_FILE_BINARY 4
void tf_file_open(const tf_string *path, int32_t mode, int32_t number);
void tf_file_close(int32_t number); /* not open: nothing to do, as VB */
void tf_file_close_all(void);
int32_t tf_file_free(void); /* FreeFile: the lowest number not in use (67 if none) */
void tf_file_print(int32_t number, const tf_string *text, int32_t newline);
tf_string *tf_file_line_input(int32_t number);
int tf_file_eof(int32_t number);
int64_t tf_file_lof(int32_t number); /* the file's length in bytes */
int64_t
tf_file_seek_pos(int32_t number); /* Seek(n): the byte offset next read or written, from 0 */

/* ------------------------------------------------------------------------ */
/* Arrays                                                                   */
/* ------------------------------------------------------------------------ */

/*
 * Every array starts at 0 and stores its elements column-major (the first
 * index varies fastest), as VB does, so ReDim Preserve of the last
 * dimension never moves existing elements. A fixed array (Dim a(10)) is
 * sized once; a dynamic one (Dim a()) starts empty and is sized by ReDim.
 * Generated code owns tf_array values directly (as locals, globals) and
 * passes them by pointer; a zeroed tf_array is a valid empty array.
 *
 * Errors: an index out of range is error 9 (and the element returned is a
 * scratch one, so a statement that goes on to its error check harms
 * nothing); ReDim or Erase of a fixed array passed ByRef, or of an array
 * locked because one of its elements is being passed ByRef, is error 10.
 */
#define TF_ARRAY_MAX_RANK 8

typedef enum tf_elem
{
    TF_ELEM_I32,    /* Integer, Integer-based Enum */
    TF_ELEM_I64,    /* Long, Long-based Enum */
    TF_ELEM_F64,    /* Double */
    TF_ELEM_BOOL,   /* Boolean (int) */
    TF_ELEM_STRING, /* tf_string *, released when replaced or freed */
    TF_ELEM_RECORD, /* a user Type, handled through its tf_record_ops */
    TF_ELEM_REF,    /* a Ref to a user Type (see "Refs" below) */
    TF_ELEM_U8      /* Byte (uint8_t) */
} tf_elem;

/*
 * What the runtime needs to know about a user Type to keep records in
 * arrays: generated code supplies one of these per Type. init makes a
 * zeroed record ready (fixed arrays inside sized); free releases what it
 * holds; copy makes `to` (already initialised) a deep copy of `from`.
 */
typedef struct tf_record_ops
{
    size_t size;
    void (*init)(void *record);
    void (*free)(void *record);
    void (*copy)(void *to, const void *from);
} tf_record_ops;

typedef struct tf_array
{
    void *data;               /* NULL when empty */
    int64_t count;            /* elements */
    int32_t rank;             /* dimensions, 0 when empty */
    int32_t elem;             /* tf_elem */
    int32_t fixed;            /* Dim a(10): cannot be ReDim'd or Erased away */
    int32_t locks;            /* elements currently passed ByRef */
    const tf_record_ops *ops; /* TF_ELEM_RECORD only */
    int32_t upper[TF_ARRAY_MAX_RANK];
    struct tf_dict_index *dict; /* a Dictionary's keys; NULL for an array */
} tf_array;

/* A fixed array, all elements 0 / "" / False (records: their init). ops is
   NULL except for TF_ELEM_RECORD. */
void tf_array_init(tf_array *a, tf_elem elem, const tf_record_ops *ops, int32_t rank,
                   const int32_t *upper);

/* ReDim [Preserve]: an upper bound of -1 gives an empty dimension. */
void tf_array_redim(tf_array *a, tf_elem elem, const tf_record_ops *ops, int preserve, int32_t rank,
                    const int32_t *upper);

/* Erase: a dynamic array becomes empty; a fixed one keeps its size and has
   its elements reset. A Dim of the array inside a loop, run again, does
   the same. */
void tf_array_erase(tf_array *a);

/* Releases everything the array holds (at the procedure's end). */
void tf_array_free(tf_array *a);

/* to = from: to (dynamic) becomes a copy; strings are shared by reference,
   records deep-copied. */
void tf_array_copy(tf_array *to, const tf_array *from);

/* The same, for an array inside a record being copied: to may be fixed (it
   then has from's shape already). */
void tf_array_assign(tf_array *to, const tf_array *from);

/* The address of an element, after checking each index (error 9). */
void *tf_array_at1(tf_array *a, int64_t i);
void *tf_array_at2(tf_array *a, int64_t i, int64_t j);
void *tf_array_atn(tf_array *a, int32_t rank, const int64_t *index);

/* For Each: how many elements, and element i (0..count-1) in storage
   order (column-major, as the elements are kept); error 9 outside. */
int64_t tf_array_count(const tf_array *a);
void *tf_array_atlin(tf_array *a, int64_t i);

/* LBound (always 0) and UBound of dimension 1..rank; error 9 if the array
   is empty or has no such dimension. */
int32_t tf_array_lbound(const tf_array *a, int32_t dimension);
int32_t tf_array_ubound(const tf_array *a, int32_t dimension);

/* Around a call that receives an element ByRef: ReDim and Erase of the
   array are error 10 meanwhile, so the element's address stays valid. */
void tf_array_lock(tf_array *a);
void tf_array_unlock(tf_array *a);

/* String functions on arrays. Split into to (a dynamic String array; error
   10 if fixed or locked): "" gives no elements (UBound -1), a delimiter ""
   the whole string. Join: a String array's elements, the delimiter between. */
void tf_str_split(tf_array *to, const tf_string *s, const tf_string *delimiter);
tf_string *tf_str_join(const tf_array *strings, const tf_string *delimiter);

/* Binary files (Open ... For Binary): Get reads into a Byte array, Put
   writes from one, at an offset from 0 (has_offset 0: where the last read
   or write ended), count bytes (has_count 0: the buffer's size). Get with
   a count resizes the buffer (a dynamic array); reading past the end gives
   fewer bytes: a dynamic buffer is shortened to them (UBound -1 for none),
   a fixed one has the rest set to 0. A negative offset is error 63, a
   negative count 5, a count beyond Put's buffer 9, another mode 54. */
void tf_file_get(int32_t number, int32_t has_offset, int64_t offset, tf_array *buffer,
                 int32_t has_count, int32_t count);
void tf_file_put(int32_t number, int32_t has_offset, int64_t offset, const tf_array *buffer,
                 int32_t has_count, int32_t count);
void tf_file_seek(int32_t number, int64_t offset);

/* ------------------------------------------------------------------------ */
/* Dictionaries: String keys, values of one type                            */
/* ------------------------------------------------------------------------ */

/*
 * A Dictionary is a tf_array of rank 1 whose elements are its values, in
 * the order their keys were first added, plus an index of those keys
 * (case-sensitive, compared byte for byte). So it is declared, freed,
 * copied, locked and held in records exactly as a dynamic array; a zeroed
 * tf_array is an empty Dictionary.
 *
 * Adding a key may move the values in memory (as ReDim may move an
 * array's), so a value's address is taken only after anything that could
 * add to the same Dictionary, and adding or removing while a value is
 * passed ByRef (the Dictionary locked) is error 10.
 *
 * get: the value for key; error 5 if there is none. put: the same, adding
 * the key (value 0 / "" / a new record) if it is not there. add: adds the
 * key, error 457 if it is there. Errors return a scratch value, as for
 * arrays. elem and ops describe the values (ops for TF_ELEM_RECORD): a
 * Dictionary nothing has been added to does not know them yet.
 */
void *tf_dict_get(tf_array *d, tf_elem elem, const tf_record_ops *ops, const tf_string *key);
void *tf_dict_put(tf_array *d, tf_elem elem, const tf_record_ops *ops, tf_string *key);
void *tf_dict_add(tf_array *d, tf_elem elem, const tf_record_ops *ops, tf_string *key);
int tf_dict_exists(const tf_array *d, const tf_string *key);
int32_t tf_dict_count(const tf_array *d);
void tf_dict_remove(tf_array *d, const tf_string *key); /* error 5 if not there */
void tf_dict_clear(tf_array *d);

/* Keys and Items: copies into a dynamic array (rank 1; UBound -1 when the
   Dictionary is empty), in insertion order. Error 10 if to is fixed or
   locked, as for any array assignment. */
void tf_dict_keys(tf_array *to, const tf_array *d);
void tf_dict_items(tf_array *to, const tf_array *d, tf_elem elem, const tf_record_ops *ops);

/* ------------------------------------------------------------------------ */
/* Refs: reference-counted records                                           */
/* ------------------------------------------------------------------------ */

/*
 * A Ref (Ref Customer) points to a record on the heap, behind a header that
 * holds its reference count and its Type's tf_record_ops; NULL is Nothing.
 * Ownership follows strings: a function documented "+1" hands the caller
 * a reference to release; arguments are borrowed. Cycles are never freed
 * (decided 23 Oct 2026).
 */

/* New Customer: a fresh record, initialised. +1 */
void *tf_ref_new(const tf_record_ops *ops);

/* A new record holding a deep copy of *from (r = c, copying a value). +1 */
void *tf_ref_new_copy(const tf_record_ops *ops, const void *from);

void tf_ref_retain(void *record);
void tf_ref_release(void *record);

/* Takes a reference to a borrowed Ref and returns it. +1 */
void *tf_ref_ref(void *record);

/* old is the variable's current value, owned the new one (+1): releases
   old and returns owned, as in "v = tf_ref_replace(v, tf_ref_ref(x))". */
void *tf_ref_replace(void *old, void *owned);

/* The record a Ref points to, or, for Nothing, error 91 and a scratch
   record of the Type (so a statement going on to its check harms nothing). */
void *tf_ref_get(void *record, const tf_record_ops *ops);

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
