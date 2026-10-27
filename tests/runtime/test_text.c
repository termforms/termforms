/* SPDX-License-Identifier: 0BSD */
/*
 * test_text.c -- the string functions: positions counted in code points
 * (multi-byte UTF-8 included) and starting at 1, VB's edge cases (an
 * empty search string, a start past the end, InStrRev's limit), and
 * error 5 for bad arguments, raised catchably.
 */
#include "termforms/tf_runtime.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

/* Arguments, kept to be released at the end (the sanitizer jobs check
   for leaks). */
static tf_string *pool[64];
static int pooled = 0;

static tf_string *text(const char *s)
{
    tf_string *t = tf_str_from_utf8(s, (int32_t)strlen(s));
    pool[pooled++] = t;
    return t;
}

/* Compares an owned result with what is expected, then releases it. */
static void expect_text(tf_string *got, const char *want, const char *what)
{
    if (strcmp(tf_str_cstr(got), want) != 0)
    {
        fprintf(stderr, "FAIL: %s: got \"%s\", want \"%s\"\n", what, tf_str_cstr(got), want);
        ++failures;
    }
    tf_str_release(got);
}

static void expect_int(int32_t got, int32_t want, const char *what)
{
    if (got != want)
    {
        fprintf(stderr, "FAIL: %s: got %d, want %d\n", what, (int)got, (int)want);
        ++failures;
    }
}

int main(void)
{
    tf_string *hello = text("Hello");
    tf_string *greek = text("\xce\xb1\xce\xb2\xce\xb3x"); /* "αβγx": 4 code points, 7 bytes */
    tf_string *l = text("l");

    expect_text(tf_str_mid(hello, 2, 3), "ell", "Mid$ from 2, 3 long");
    expect_text(tf_str_mid(hello, 4, TF_STR_REST), "lo", "Mid$ to the end");
    expect_text(tf_str_mid(hello, 9, 2), "", "Mid$ past the end");
    expect_text(tf_str_mid(greek, 2, 2), "\xce\xb2\xce\xb3", "Mid$ counts code points");
    expect_text(tf_str_left(greek, 1), "\xce\xb1", "Left$ of UTF-8");
    expect_text(tf_str_right(greek, 2), "\xce\xb3x", "Right$ of UTF-8");
    expect_text(tf_str_right(hello, 99), "Hello", "Right$ longer than the string");

    expect_int(tf_str_instr(1, hello, l), 3, "InStr");
    expect_int(tf_str_instr(4, hello, l), 4, "InStr from 4");
    expect_int(tf_str_instr(1, hello, text("z")), 0, "InStr not found");
    expect_int(tf_str_instr(2, hello, NULL), 2, "InStr of \"\" is start");
    expect_int(tf_str_instr(6, hello, NULL), 0, "InStr past the end");
    tf_string *x = text("x");
    expect_int(tf_str_instr(1, greek, x), 4, "InStr counts code points");
    expect_int(tf_str_instrrev(hello, l, -1), 4, "InStrRev");
    expect_int(tf_str_instrrev(text("abcabc"), text("bc"), 5), 2,
               "InStrRev: match must end by start");

    expect_text(tf_str_ucase(text("ab\xc3\xa9Z")), "AB\xc3\xa9Z", "UCase$ leaves non-ASCII alone");
    expect_text(tf_str_lcase(hello), "hello", "LCase$");
    expect_text(tf_str_trim(text("  a b  ")), "a b", "Trim$");
    expect_text(tf_str_ltrim(text("\t a")), "\t a", "LTrim$ trims spaces only");
    expect_text(tf_str_space(3), "   ", "Space$");
    expect_text(tf_str_repeat(3, greek), "\xce\xb1\xce\xb1\xce\xb1",
                "String$ repeats the first character");
    expect_text(tf_str_chr(0x20AC), "\xe2\x82\xac", "Chr$ of the euro sign");
    expect_int(tf_str_asc(text("\xe2\x82\xac")), 0x20AC, "Asc of the euro sign");
    expect_text(tf_str_replace(text("a.b.c"), text("."), text("--")), "a--b--c", "Replace");
    expect_text(tf_str_replace(hello, NULL, x), "Hello", "Replace of \"\" changes nothing");
    expect_text(tf_str_reverse(greek), "x\xce\xb3\xce\xb2\xce\xb1",
                "StrReverse keeps characters whole");

    tf_array parts;
    memset(&parts, 0, sizeof parts);
    tf_str_split(&parts, text("a,,b"), text(","));
    expect_int((int32_t)parts.count, 3, "Split keeps empty pieces");
    expect_text(tf_str_join(&parts, text("+")), "a++b", "Join");
    tf_str_split(&parts, NULL, text(","));
    expect_int(tf_array_ubound(&parts, 1), -1, "Split of \"\": UBound -1");
    tf_str_split(&parts, hello, NULL);
    expect_int((int32_t)parts.count, 1, "Split with delimiter \"\": the whole string");

    tf_try_enter();
    expect_text(tf_str_mid(hello, 0, 1), "", "Mid$ from 0");
    expect_int(tf_error_pending && tf_err_number() == 5, 1, "Mid$ from 0 is error 5");
    tf_err_catch();
    expect_text(tf_str_left(hello, -1), "", "Left$ of -1");
    expect_int(tf_error_pending && tf_err_number() == 5, 1, "Left$ of -1 is error 5");
    tf_err_catch();
    expect_int(tf_str_asc(NULL), 0, "Asc of \"\"");
    expect_int(tf_error_pending && tf_err_number() == 5, 1, "Asc of \"\" is error 5");
    tf_err_catch();
    expect_text(tf_str_chr(0xD800), "", "Chr$ of a surrogate");
    expect_int(tf_error_pending && tf_err_number() == 5, 1, "Chr$ of a surrogate is error 5");
    tf_err_catch();
    tf_try_leave();

    tf_array_free(&parts);
    for (int i = 0; i < pooled; ++i)
    {
        tf_str_release(pool[i]);
    }
    if (failures != 0)
    {
        return 1;
    }
    printf("PASS: string functions\n");
    return 0;
}
