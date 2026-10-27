/* SPDX-License-Identifier: 0BSD */
/*
 * test_numbers.c -- number formatting, rounding and parsing in the runtime:
 * the rules decided on 27 Oct 2026 (shortest round-trip text for Doubles,
 * Str$'s leading space, round-half-to-even, Val's leniency). Error paths
 * end the program, so they are covered by run tests instead.
 */
#include "termforms/tf_runtime.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static tf_string *text(const char *s)
{
    return tf_str_from_utf8(s, (int32_t)strlen(s));
}

/* Compares and releases an owned string. */
static void expect_text(tf_string *actual, const char *expected, const char *what)
{
    if (strcmp(tf_str_cstr(actual), expected) != 0)
    {
        fprintf(stderr, "FAIL: %s: expected \"%s\", got \"%s\"\n", what, expected,
                tf_str_cstr(actual));
        ++failures;
    }
    tf_str_release(actual);
}

static void expect_double(double actual, double expected, const char *what)
{
    if (actual != expected)
    {
        fprintf(stderr, "FAIL: %s: expected %.17g, got %.17g\n", what, expected, actual);
        ++failures;
    }
}

static void expect_long(int64_t actual, int64_t expected, const char *what)
{
    if (actual != expected)
    {
        fprintf(stderr, "FAIL: %s: expected %lld, got %lld\n", what, (long long)expected,
                (long long)actual);
        ++failures;
    }
}

int main(void)
{
    /* Doubles print as the shortest text that reads back exactly. */
    expect_text(tf_str_from_f64(0.1), "0.1", "0.1");
    expect_text(tf_str_from_f64(3.0), "3", "3.0");
    expect_text(tf_str_from_f64(-2.5), "-2.5", "-2.5");
    expect_text(tf_str_from_f64(1.0 / 3.0), "0.3333333333333333", "1/3");
    expect_text(tf_str_from_f64(0.1 + 0.2), "0.30000000000000004", "0.1+0.2");
    expect_text(tf_str_from_f64(123456789012345.0), "123456789012345", "below 1E15");
    expect_text(tf_str_from_f64(1e15), "1E+15", "1E15");
    expect_text(tf_str_from_f64(1.5e20), "1.5E+20", "1.5E20");
    expect_text(tf_str_from_f64(0.0001), "0.0001", "0.0001");
    expect_text(tf_str_from_f64(0.00001), "1E-05", "0.00001");
    expect_text(tf_str_from_f64(-1.25e-7), "-1.25E-07", "-1.25E-7");
    expect_text(tf_str_from_f64(1e300), "1E+300", "1E300");
    expect_text(tf_str_from_f64(-0.0), "0", "-0");
    expect_text(tf_str_from_f64(1234.5), "1234.5", "1234.5");

    /* Long text, including the one value that cannot be negated. */
    expect_text(tf_str_from_i64(INT64_MIN), "-9223372036854775808", "INT64_MIN");
    expect_text(tf_str_from_i64(INT64_MAX), "9223372036854775807", "INT64_MAX");
    expect_text(tf_str_from_i64(0), "0", "0");

    /* Str$ keeps VB's leading space for zero and positive numbers. */
    expect_text(tf_str_str_i64(5), " 5", "Str$(5)");
    expect_text(tf_str_str_i64(-5), "-5", "Str$(-5)");
    expect_text(tf_str_str_i64(0), " 0", "Str$(0)");
    expect_text(tf_str_str_f64(0.5), " 0.5", "Str$(0.5)");
    expect_text(tf_str_str_f64(-0.5), "-0.5", "Str$(-0.5)");

    /* CInt/CLng round half to even. */
    expect_long(tf_i32_from_f64(2.5), 2, "CInt(2.5)");
    expect_long(tf_i32_from_f64(3.5), 4, "CInt(3.5)");
    expect_long(tf_i32_from_f64(-2.5), -2, "CInt(-2.5)");
    expect_long(tf_i32_from_f64(-3.5), -4, "CInt(-3.5)");
    expect_long(tf_i32_from_f64(2.6), 3, "CInt(2.6)");
    expect_long(tf_i32_from_f64(-2.4), -2, "CInt(-2.4)");
    expect_long(tf_i32_from_f64(2147483647.4), 2147483647, "CInt near the top");
    expect_long(tf_i64_from_f64(1e18), 1000000000000000000, "CLng(1E18)");
    expect_long(tf_i64_from_f64(-9223372036854775808.0), INT64_MIN, "CLng(-2^63)");

    /* Fix truncates, Int floors. */
    expect_double(tf_f64_fix(-2.7), -2.0, "Fix(-2.7)");
    expect_double(tf_f64_int(-2.7), -3.0, "Int(-2.7)");
    expect_double(tf_f64_int(2.7), 2.0, "Int(2.7)");
    expect_double(tf_f64_int(-3.0), -3.0, "Int(-3)");

    /* Mod's sign follows the dividend, in every type. */
    expect_long(tf_i32_mod(-7, 3), -1, "-7 Mod 3");
    expect_long(tf_i32_mod(INT32_MIN, -1), 0, "INT32_MIN Mod -1");
    expect_long(tf_i64_mod(INT64_MIN, -1), 0, "INT64_MIN Mod -1");
    expect_double(tf_f64_mod(5.5, 2.0), 1.5, "5.5 Mod 2");

    /* Long arithmetic at the edges of the range. */
    expect_long(tf_i64_add(INT64_MAX - 1, 1), INT64_MAX, "add to the top");
    expect_long(tf_i64_mul(-3037000499, 3037000499), -9223372030926249001, "mul near the top");
    expect_long(tf_i64_sub(INT64_MIN + 1, 1), INT64_MIN, "sub to the bottom");

    /* Text to numbers. CInt/CLng/CDbl want the whole text to be a number. */
    tf_string *s;
    expect_double(tf_f64_from_str(s = text("  -1.5e3 ")), -1500.0, "CDbl(\" -1.5e3 \")");
    tf_str_release(s);
    expect_long(tf_i32_from_str(s = text("2.5")), 2, "CInt(\"2.5\")");
    tf_str_release(s);
    expect_long(tf_i64_from_str(s = text("9223372036854775807")), INT64_MAX, "CLng exact");
    tf_str_release(s);
    expect_long(tf_i64_from_str(s = text("-9223372036854775808")), INT64_MIN, "CLng min");
    tf_str_release(s);
    expect_long(tf_i32_from_str(s = text("&HFF")), 255, "CInt(\"&HFF\")");
    tf_str_release(s);

    /* Val is lenient: it skips spaces and stops at the first stray character. */
    expect_double(tf_val(s = text(" 1 2 3abc")), 123.0, "Val(\" 1 2 3abc\")");
    tf_str_release(s);
    expect_double(tf_val(s = text("2.5E")), 2.5, "Val(\"2.5E\")");
    tf_str_release(s);
    expect_double(tf_val(s = text("1D2")), 100.0, "Val(\"1D2\")");
    tf_str_release(s);
    expect_double(tf_val(s = text("&O17")), 15.0, "Val(\"&O17\")");
    tf_str_release(s);
    expect_double(tf_val(s = text("abc")), 0.0, "Val(\"abc\")");
    tf_str_release(s);
    expect_double(tf_val(NULL), 0.0, "Val(\"\")");

    if (failures != 0)
    {
        return 1;
    }
    printf("PASS: numbers\n");
    return 0;
}
