/* SPDX-License-Identifier: 0BSD */
/*
 * tf_integer.c -- Integer (32-bit) and Long (64-bit) arithmetic that raises
 * a runtime error on overflow instead of wrapping.
 *
 * Integer operations are done in 64 bits and range-checked. Long operations
 * test their operands before operating, since there is no wider type to
 * fall back on in C99 (and signed overflow is undefined behaviour). Both are
 * portable C99 with no compiler builtins, so MSVC, gcc, clang and zig cc
 * all agree.
 *
 * tf_raise returns when a Try can catch the error, so every failing path
 * returns 0 without doing the operation that would overflow.
 */
#include "../tf_internal.h"

static int32_t overflow32(void)
{
    tf_raise(TF_ERR_OVERFLOW, NULL);
    return 0;
}

static int64_t overflow64(void)
{
    tf_raise(TF_ERR_OVERFLOW, NULL);
    return 0;
}

static int division_by_zero(void)
{
    tf_raise(TF_ERR_DIVISION_BY_ZERO, NULL);
    return 0;
}

static int32_t checked(int64_t result)
{
    if (result < INT32_MIN || result > INT32_MAX)
    {
        return overflow32();
    }
    return (int32_t)result;
}

int32_t tf_i32_add(int32_t a, int32_t b)
{
    return checked((int64_t)a + b);
}

int32_t tf_i32_sub(int32_t a, int32_t b)
{
    return checked((int64_t)a - b);
}

int32_t tf_i32_mul(int32_t a, int32_t b)
{
    return checked((int64_t)a * b);
}

int32_t tf_i32_div(int32_t a, int32_t b)
{
    if (b == 0)
    {
        return division_by_zero();
    }
    /* INT32_MIN / -1 is the one quotient that does not fit. */
    return checked((int64_t)a / b);
}

int32_t tf_i32_neg(int32_t a)
{
    return checked(-(int64_t)a);
}

int32_t tf_i32_mod(int32_t a, int32_t b)
{
    if (b == 0)
    {
        return division_by_zero();
    }
    /* In 64 bits INT32_MIN % -1 is defined (0); C's % already truncates,
       so the sign follows a, as VB's Mod does. */
    return (int32_t)((int64_t)a % b);
}

int32_t tf_i32_from_i64(int64_t value)
{
    return checked(value);
}

int64_t tf_i64_add(int64_t a, int64_t b)
{
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
    {
        return overflow64();
    }
    return a + b;
}

int64_t tf_i64_sub(int64_t a, int64_t b)
{
    if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b))
    {
        return overflow64();
    }
    return a - b;
}

int64_t tf_i64_mul(int64_t a, int64_t b)
{
    /* Each sign combination has one bound to test (CERT INT32-C). */
    if (a > 0)
    {
        if ((b > 0 && a > INT64_MAX / b) || (b < 0 && b < INT64_MIN / a))
        {
            return overflow64();
        }
    }
    else if (a < 0)
    {
        if ((b > 0 && a < INT64_MIN / b) || (b < 0 && b < INT64_MAX / a))
        {
            return overflow64();
        }
    }
    return a * b;
}

int64_t tf_i64_div(int64_t a, int64_t b)
{
    if (b == 0)
    {
        return division_by_zero();
    }
    if (a == INT64_MIN && b == -1)
    {
        return overflow64();
    }
    return a / b;
}

int64_t tf_i64_mod(int64_t a, int64_t b)
{
    if (b == 0)
    {
        return division_by_zero();
    }
    /* INT64_MIN % -1 is undefined in C; mathematically it is 0. */
    return b == -1 ? 0 : a % b;
}

int64_t tf_i64_neg(int64_t a)
{
    if (a == INT64_MIN)
    {
        return overflow64();
    }
    return -a;
}

/* Bytes: 0 to 255. Arithmetic on them is done as Integers (the compiler
   widens them), so only narrowing back can overflow. */
uint8_t tf_u8_from_i64(int64_t value)
{
    if (value < 0 || value > 255)
    {
        tf_raise(TF_ERR_OVERFLOW, NULL);
        return 0;
    }
    return (uint8_t)value;
}

uint8_t tf_u8_from_i32(int32_t value)
{
    return tf_u8_from_i64(value);
}

uint8_t tf_u8_from_f64(double value)
{
    /* CInt's rounding first (half to even); out of its range is error 6 too. */
    const int32_t whole = tf_i32_from_f64(value);
    return tf_error_pending ? 0 : tf_u8_from_i64(whole);
}

uint8_t tf_u8_from_str(const tf_string *text)
{
    const int32_t whole = tf_i32_from_str(text);
    return tf_error_pending ? 0 : tf_u8_from_i64(whole);
}

uint8_t tf_u8_add(int32_t a, int32_t b)
{
    return tf_u8_from_i64((int64_t)a + b);
}
