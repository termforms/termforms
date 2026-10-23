/* SPDX-License-Identifier: 0BSD */
/*
 * tf_integer.c -- 32-bit Integer arithmetic that raises a runtime error on
 * overflow instead of wrapping.
 *
 * Each operation is done in 64 bits and range-checked. That is portable C99
 * (no compiler builtins, so MSVC, gcc, clang and zig cc all agree) and the
 * product of two 32-bit values always fits in 64 bits.
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
