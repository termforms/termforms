/* SPDX-License-Identifier: 0BSD */
/*
 * tf_double.c -- Double arithmetic and conversions between Double and the
 * integer types.
 *
 * Doubles never hold Infinity or NaN (a decision of 27 Oct 2026, as VB6):
 * every operation that could produce one raises a runtime error instead, so
 * a bad value stops the program at the line that made it rather than
 * spreading. Since the operands are always finite, checking the result is
 * enough.
 *
 * Rounding to Integer/Long is round-half-to-even, as VB's CInt and CLng.
 * It is done by hand rather than with rint(), which depends on the floating
 * point rounding mode.
 */
#include "../tf_internal.h"

#include <math.h>

/* 2^52: from here up every Double is a whole number. */
#define TF_F64_INTEGRAL 4503599627370496.0

static double finite(double result)
{
    if (!isfinite(result))
    {
        tf_runtime_error(TF_ERR_OVERFLOW, "Overflow");
    }
    return result;
}

double tf_f64_add(double a, double b)
{
    return finite(a + b);
}

double tf_f64_sub(double a, double b)
{
    return finite(a - b);
}

double tf_f64_mul(double a, double b)
{
    return finite(a * b);
}

double tf_f64_div(double a, double b)
{
    if (b == 0.0)
    {
        tf_runtime_error(TF_ERR_DIVISION_BY_ZERO, "Division by zero");
    }
    return finite(a / b);
}

double tf_f64_mod(double a, double b)
{
    if (b == 0.0)
    {
        tf_runtime_error(TF_ERR_DIVISION_BY_ZERO, "Division by zero");
    }
    return fmod(a, b); /* exact; the sign follows a, as VB's Mod */
}

double tf_f64_pow(double a, double b)
{
    const double result = pow(a, b);
    if (isnan(result))
    {
        /* A negative number to a fractional power, as VB6 reports it. */
        tf_runtime_error(TF_ERR_INVALID_ARGUMENT, "Invalid procedure call or argument");
    }
    if (a == 0.0 && b < 0.0)
    {
        tf_runtime_error(TF_ERR_DIVISION_BY_ZERO, "Division by zero");
    }
    return finite(result);
}

double tf_f64_fix(double value)
{
    if (value >= TF_F64_INTEGRAL || value <= -TF_F64_INTEGRAL)
    {
        return value;
    }
    return (double)(int64_t)value; /* the cast truncates towards zero */
}

double tf_f64_int(double value)
{
    const double t = tf_f64_fix(value);
    return t > value ? t - 1.0 : t;
}

/* Nearest whole number, halves to the even neighbour (2.5 -> 2, -3.5 -> -4). */
static double round_half_even(double value)
{
    if (value >= TF_F64_INTEGRAL || value <= -TF_F64_INTEGRAL)
    {
        return value;
    }
    const double t = tf_f64_fix(value);
    const double diff = value - t; /* exact: a fractional part is representable */
    const int odd = (int64_t)t % 2 != 0;
    if (diff > 0.5 || (diff == 0.5 && odd))
    {
        return t + 1.0;
    }
    if (diff < -0.5 || (diff == -0.5 && odd))
    {
        return t - 1.0;
    }
    return t;
}

int32_t tf_i32_from_f64(double value)
{
    const double r = round_half_even(value);
    if (r < -2147483648.0 || r > 2147483647.0)
    {
        tf_runtime_error(TF_ERR_OVERFLOW, "Overflow");
    }
    return (int32_t)r;
}

int64_t tf_i64_from_f64(double value)
{
    const double r = round_half_even(value);
    /* 2^63 is exactly representable; INT64_MAX is not, so compare with it. */
    if (r < -9223372036854775808.0 || r >= 9223372036854775808.0)
    {
        tf_runtime_error(TF_ERR_OVERFLOW, "Overflow");
    }
    return (int64_t)r;
}
