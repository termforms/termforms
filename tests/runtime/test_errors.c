/* SPDX-License-Identifier: 0BSD */
/*
 * test_errors.c -- error numbers and their standard texts follow the policy
 * of 27 Oct 2026: VB6's number and VB6's exact wording where VB6 has the
 * error, TermForms-only errors from 900, programs' own errors from 1000.
 */
#include "termforms/tf_runtime.h"

/* For the string header: the too-long test fakes two huge strings. */
#include "../../runtime/src/tf_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void expect(int32_t number, int32_t expected_number, const char *expected_text)
{
    if (number != expected_number)
    {
        fprintf(stderr, "FAIL: constant is %ld, expected %ld\n", (long)number,
                (long)expected_number);
        ++failures;
    }
    if (strcmp(tf_error_text(number), expected_text) != 0)
    {
        fprintf(stderr, "FAIL: error %ld: expected \"%s\", got \"%s\"\n", (long)number,
                expected_text, tf_error_text(number));
        ++failures;
    }
}

int main(void)
{
    /* VB6's numbers and wording, character for character. */
    expect(TF_ERR_INVALID_ARGUMENT, 5, "Invalid procedure call or argument");
    expect(TF_ERR_OVERFLOW, 6, "Overflow");
    expect(TF_ERR_OUT_OF_MEMORY, 7, "Out of memory");
    expect(TF_ERR_DIVISION_BY_ZERO, 11, "Division by zero");
    expect(TF_ERR_TYPE_MISMATCH, 13, "Type mismatch");
    expect(TF_ERR_OUT_OF_STRING_SPACE, 14, "Out of string space");

    /* TermForms-only errors live in 900-999, below the programs' range. */
    expect(TF_ERR_TERMINAL, 900, "Terminal error");
    if (TF_ERR_TERMINAL < 900 || TF_ERR_TERMINAL >= TF_ERR_FIRST_USER)
    {
        fprintf(stderr, "FAIL: TF_ERR_TERMINAL is outside 900-999\n");
        ++failures;
    }

    /* Numbers with no meaning of their own get VB's generic text. */
    expect(TF_ERR_FIRST_USER, 1000, "Application-defined or object-defined error");
    expect(3, 3, "Application-defined or object-defined error"); /* GoSub: never used */

    /* Joining two strings too long to combine is error 14, which a Try can
       catch: concatenation checks the lengths before it touches the text
       or allocates, so two headers that only claim to be 1.5 GB each stand
       in for real ones. */
    tf_string *huge = malloc(sizeof(tf_string) + 1);
    huge->refcount = TF_STR_IMMORTAL;
    huge->length = 1500000000;
    tf_try_enter();
    tf_string *joined = tf_str_concat(huge, huge);
    if (joined != NULL || !tf_error_pending || tf_err_number() != 14 ||
        strcmp(tf_str_cstr(tf_err_description()), "Out of string space") != 0)
    {
        fprintf(stderr, "FAIL: a too-long concatenation did not raise a catchable error 14\n");
        ++failures;
    }
    tf_err_catch();
    tf_try_leave();
    free(huge);

    if (failures != 0)
    {
        return 1;
    }
    printf("PASS: error numbers\n");
    return 0;
}
