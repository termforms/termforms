/* SPDX-License-Identifier: 0BSD */
/*
 * test_errors.c -- error numbers and their standard texts follow the policy
 * of 27 Oct 2026: VB6's number and VB6's exact wording where VB6 has the
 * error, TermForms-only errors from 900, programs' own errors from 1000.
 */
#include "termforms/tf_runtime.h"

#include <stdio.h>
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

    if (failures != 0)
    {
        return 1;
    }
    printf("PASS: error numbers\n");
    return 0;
}
