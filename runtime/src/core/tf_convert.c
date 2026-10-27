/* SPDX-License-Identifier: 0BSD */
/*
 * tf_convert.c -- numbers to text and text to numbers: the & operator,
 * CStr, Str$, Val, and CInt/CLng/CDbl applied to a String.
 *
 * The runtime never calls setlocale(), so snprintf and strtod use the "C"
 * locale and the decimal separator is always '.', as in VB source code.
 * Integers are formatted by hand: older MinGW C libraries do not
 * understand %lld.
 */
#include "../tf_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Numbers to text                                                          */
/* ------------------------------------------------------------------------ */

static tf_string *integer_text(int64_t value, int leading_space)
{
    char buffer[24];
    char *p = buffer + sizeof buffer;
    /* Unsigned arithmetic: -INT64_MIN does not fit in an int64_t. */
    uint64_t magnitude = value < 0 ? 0u - (uint64_t)value : (uint64_t)value;
    do
    {
        *--p = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0u);
    if (value < 0)
    {
        *--p = '-';
    }
    else if (leading_space)
    {
        *--p = ' ';
    }
    return tf_str_from_utf8(p, (int32_t)(buffer + sizeof buffer - p));
}

/*
 * Writes the shortest decimal text that reads back as exactly value. Fixed
 * notation from 0.0001 up to below 1E15, else d.dddE+XX (VB.NET's choice).
 * Returns the length.
 */
static int double_text(double value, char *out, int leading_space)
{
    char *o = out;
    if (value == 0.0) /* also -0, which VB prints as 0 */
    {
        if (leading_space)
        {
            *o++ = ' ';
        }
        *o++ = '0';
        return (int)(o - out);
    }

    /* The fewest significant digits that survive a round trip. */
    char sci[40];
    for (int precision = 1; precision <= 17; ++precision)
    {
        snprintf(sci, sizeof sci, "%.*e", precision - 1, value);
        if (strtod(sci, NULL) == value)
        {
            break;
        }
    }

    /* sci is "[-]d[.ddd]e[+-]x...": collect the digits and the exponent. */
    const char *p = sci;
    if (*p == '-')
    {
        *o++ = '-';
        ++p;
    }
    else if (leading_space)
    {
        *o++ = ' ';
    }
    char digits[20];
    int n = 0;
    for (; *p != 'e' && *p != 'E'; ++p)
    {
        if (*p != '.')
        {
            digits[n++] = *p;
        }
    }
    const int exponent = (int)strtol(p + 1, NULL, 10);
    while (n > 1 && digits[n - 1] == '0')
    {
        --n;
    }

    if (exponent < -4 || exponent >= 15)
    {
        *o++ = digits[0];
        if (n > 1)
        {
            *o++ = '.';
            memcpy(o, digits + 1, (size_t)(n - 1));
            o += n - 1;
        }
        /* At most "E+308" and its NUL. */
        o += snprintf(o, 8, "E%c%02d", exponent < 0 ? '-' : '+',
                      exponent < 0 ? -exponent : exponent);
    }
    else if (exponent >= 0)
    {
        for (int i = 0; i <= exponent || i < n; ++i)
        {
            if (i == exponent + 1)
            {
                *o++ = '.';
            }
            *o++ = i < n ? digits[i] : '0';
        }
    }
    else
    {
        *o++ = '0';
        *o++ = '.';
        for (int i = 0; i < -exponent - 1; ++i)
        {
            *o++ = '0';
        }
        memcpy(o, digits, (size_t)n);
        o += n;
    }
    return (int)(o - out);
}

tf_string *tf_str_from_i64(int64_t value)
{
    return integer_text(value, 0);
}

tf_string *tf_str_str_i64(int64_t value)
{
    return integer_text(value, 1);
}

tf_string *tf_str_from_f64(double value)
{
    char buffer[48];
    const int n = double_text(value, buffer, 0);
    return tf_str_from_utf8(buffer, (int32_t)n);
}

tf_string *tf_str_str_f64(double value)
{
    char buffer[48];
    const int n = double_text(value, buffer, 1);
    return tf_str_from_utf8(buffer, (int32_t)n);
}

/* ------------------------------------------------------------------------ */
/* Text to numbers                                                          */
/* ------------------------------------------------------------------------ */

typedef struct number
{
    double real;       /* the value as a Double */
    int64_t whole;     /* the exact value, when is_whole */
    int is_whole;      /* written without '.' or an exponent */
    int whole_too_big; /* is_whole, but outside the Long range */
} number;

static int is_digit_in(char c, int radix)
{
    if (radix == 16)
    {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }
    return c >= '0' && c < '0' + radix;
}

static int digit_value(char c)
{
    return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10;
}

/*
 * Reads the longest number at the start of text: [+-]digits[.digits]
 * [E|D[+-]digits], or &H / &O followed by hexadecimal or octal digits.
 * Returns the number of characters read, 0 if there is no number.
 */
static size_t scan_number(const char *text, number *out)
{
    const char *p = text;
    out->real = 0.0;
    out->whole = 0;
    out->is_whole = 1;
    out->whole_too_big = 0;

    if (p[0] == '&' && (p[1] == 'H' || p[1] == 'h' || p[1] == 'O' || p[1] == 'o'))
    {
        const int radix = (p[1] == 'H' || p[1] == 'h') ? 16 : 8;
        const char *start = p += 2;
        uint64_t value = 0;
        for (; is_digit_in(*p, radix); ++p)
        {
            const uint64_t limit =
                ((uint64_t)INT64_MAX - (uint64_t)digit_value(*p)) / (uint64_t)radix;
            out->whole_too_big = out->whole_too_big || value > limit;
            value = value * (uint64_t)radix + (uint64_t)digit_value(*p);
        }
        if (p == start)
        {
            return 0;
        }
        out->whole = (int64_t)value;
        out->real = (double)value;
        return (size_t)(p - text);
    }

    const int negative = *p == '-';
    if (*p == '+' || *p == '-')
    {
        ++p;
    }
    int digits = 0;
    uint64_t magnitude = 0;
    const uint64_t limit = negative ? (uint64_t)INT64_MAX + 1u : (uint64_t)INT64_MAX;
    for (; *p >= '0' && *p <= '9'; ++p, ++digits)
    {
        const uint64_t d = (uint64_t)(*p - '0');
        if (magnitude > (limit - d) / 10u)
        {
            out->whole_too_big = 1;
        }
        magnitude = magnitude * 10u + d;
    }
    if (*p == '.')
    {
        out->is_whole = 0;
        for (++p; *p >= '0' && *p <= '9'; ++p, ++digits)
        {
        }
    }
    if (digits == 0)
    {
        return 0;
    }
    /* An exponent counts only if digits follow: "2E" reads as 2. */
    const char *e = p;
    if (*e == 'E' || *e == 'e' || *e == 'D' || *e == 'd')
    {
        ++e;
        if (*e == '+' || *e == '-')
        {
            ++e;
        }
        if (*e >= '0' && *e <= '9')
        {
            while (*e >= '0' && *e <= '9')
            {
                ++e;
            }
            out->is_whole = 0;
            p = e;
        }
    }

    /* strtod rounds correctly; it needs 'e' where VB also accepts 'D'. */
    const size_t length = (size_t)(p - text);
    char *copy = tf_xmalloc(length + 1);
    for (size_t i = 0; i < length; ++i)
    {
        copy[i] = (text[i] == 'D' || text[i] == 'd') ? 'e' : text[i];
    }
    copy[length] = '\0';
    out->real = strtod(copy, NULL);
    free(copy);
    if (!isfinite(out->real))
    {
        tf_raise(TF_ERR_OVERFLOW, "Overflow");
        out->real = 0.0;
    }
    out->whole = negative ? (int64_t)(0u - magnitude) : (int64_t)magnitude;
    return length;
}

static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* CInt/CLng/CDbl from text: the whole text, less surrounding spaces, must
   be one number. */
static number parse_whole_text(const tf_string *text)
{
    const char *s = tf_str_cstr(text);
    while (is_space(*s))
    {
        ++s;
    }
    number n;
    const size_t used = scan_number(s, &n);
    const char *rest = s + used;
    while (is_space(*rest))
    {
        ++rest;
    }
    if (used == 0 || *rest != '\0')
    {
        tf_raise(TF_ERR_TYPE_MISMATCH, "Type mismatch");
        n.real = 0.0; /* a Try may catch it: give back 0 */
        n.whole = 0;
        n.is_whole = 1;
        n.whole_too_big = 0;
    }
    return n;
}

double tf_f64_from_str(const tf_string *text)
{
    return parse_whole_text(text).real;
}

int64_t tf_i64_from_str(const tf_string *text)
{
    const number n = parse_whole_text(text);
    if (!n.is_whole)
    {
        return tf_i64_from_f64(n.real);
    }
    if (n.whole_too_big)
    {
        tf_raise(TF_ERR_OVERFLOW, "Overflow");
        return 0;
    }
    return n.whole;
}

int32_t tf_i32_from_str(const tf_string *text)
{
    const number n = parse_whole_text(text);
    if (!n.is_whole)
    {
        return tf_i32_from_f64(n.real);
    }
    if (n.whole_too_big)
    {
        tf_raise(TF_ERR_OVERFLOW, "Overflow");
        return 0;
    }
    return tf_i32_from_i64(n.whole);
}

double tf_val(const tf_string *text)
{
    /* VB's Val ignores spaces anywhere: Val("1 2 3") is 123. */
    const char *s = tf_str_cstr(text);
    const size_t length = strlen(s);
    char *packed = tf_xmalloc(length + 1);
    size_t n = 0;
    for (const char *p = s; *p != '\0'; ++p)
    {
        if (!is_space(*p))
        {
            packed[n++] = *p;
        }
    }
    packed[n] = '\0';
    number value;
    const size_t used = scan_number(packed, &value);
    free(packed);
    return used == 0 ? 0.0 : value.real;
}
