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
 */
#include "../tf_internal.h"

static int32_t checked(int64_t result)
{
    if (result < INT32_MIN || result > INT32_MAX)
    {
        tf_runtime_error(TF_ERR_OVERFLOW, "Overflow");
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
        tf_runtime_error(TF_ERR_DIVISION_BY_ZERO, "Division by zero");
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
        tf_runtime_error(TF_ERR_DIVISION_BY_ZERO, "Division by zero");
    }
    /* In 64 bits INT32_MIN % -1 is defined (0); C's % already truncates,
       so the sign follows a, as VB's Mod does. */
    return (int32_t)((int64_t)a % b);
}

int32_t tf_i32_from_i64(int64_t value)
{
    return checked(value);
}

static void overflow(void)
{
    tf_runtime_error(TF_ERR_OVERFLOW, "Overflow");
}

int64_t tf_i64_add(int64_t a, int64_t b)
{
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
    {
        overflow();
    }
    return a + b;
}

int64_t tf_i64_sub(int64_t a, int64_t b)
{
    if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b))
    {
        overflow();
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
            overflow();
        }
    }
    else if (a < 0)
    {
        if ((b > 0 && a < INT64_MIN / b) || (b < 0 && b < INT64_MAX / a))
        {
            overflow();
        }
    }
    return a * b;
}

int64_t tf_i64_div(int64_t a, int64_t b)
{
    if (b == 0)
    {
        tf_runtime_error(TF_ERR_DIVISION_BY_ZERO, "Division by zero");
    }
    if (a == INT64_MIN && b == -1)
    {
        overflow();
    }
    return a / b;
}

int64_t tf_i64_mod(int64_t a, int64_t b)
{
    if (b == 0)
    {
        tf_runtime_error(TF_ERR_DIVISION_BY_ZERO, "Division by zero");
    }
    /* INT64_MIN % -1 is undefined in C; mathematically it is 0. */
    return b == -1 ? 0 : a % b;
}

int64_t tf_i64_neg(int64_t a)
{
    if (a == INT64_MIN)
    {
        overflow();
    }
    return -a;
}
