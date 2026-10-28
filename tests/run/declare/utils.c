/* SPDX-License-Identifier: BSD-2-Clause */
/* utils.c -- the C side of tests/run/declare.bas: one function for each
   kind of parameter and result a Declare can have. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int32_t add_tax(int32_t cents)
{
    return cents + cents / 10;
}

double halve(double x)
{
    return x / 2;
}

int is_even(int64_t n)
{
    return n % 2 == 0;
}

void bump(int32_t *n)
{
    *n += 1;
}

/* VB6's buffer idiom: write into what the caller gave, NUL-terminated. */
void fill_name(char *buffer)
{
    const char *name = "TermForms";
    const size_t room = strlen(buffer);
    const size_t n = strlen(name) < room ? strlen(name) : room;
    memcpy(buffer, name, n);
    buffer[n] = '\0';
}

const char *greeting(int32_t polite)
{
    return polite ? "Good morning" : "hi";
}

const char *no_text(void)
{
    return NULL;
}

long twice_long(long n)
{
    return n * 2;
}

void triple_long(long *n)
{
    *n *= 3;
}

unsigned long bits(unsigned long v)
{
    unsigned long n = 0;
    for (; v != 0; v >>= 1)
    {
        n += v & 1;
    }
    return n;
}

typedef struct point
{
    int32_t x;
    double y;
} point;

void move_point(point *p)
{
    p->x += 1;
    p->y *= 2;
}

int32_t sum_bytes(const uint8_t *bytes, int32_t count)
{
    int32_t sum = 0;
    for (int32_t i = 0; i < count; ++i)
    {
        sum += bytes[i];
    }
    return sum;
}

/* Spelled differently in C: reached by Alias. */
int32_t side_name(void)
{
    return 42;
}
