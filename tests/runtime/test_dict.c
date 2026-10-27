/* SPDX-License-Identifier: 0BSD */
/*
 * test_dict.c -- the Dictionary runtime: keys case-sensitive and kept in
 * insertion order through growth and removal (the index rebuilt), copies
 * that do not share, Keys and Items, strings released (checked by the
 * sanitizer jobs), and errors 5, 10 and 457 raised catchably.
 */
#include "termforms/tf_runtime.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define EXPECT(cond, what)                                                                         \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            fprintf(stderr, "FAIL: %s\n", what);                                                   \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

static tf_string *text(const char *s)
{
    return tf_str_from_utf8(s, (int32_t)strlen(s));
}

/* d(key) = value for an Integer Dictionary; the key is released after, as
   generated code releases a temporary. */
static void put(tf_array *d, const char *key, int32_t value)
{
    tf_string *k = text(key);
    *(int32_t *)tf_dict_put(d, TF_ELEM_I32, NULL, k) = value;
    tf_str_release(k);
}

static int32_t get(tf_array *d, const char *key)
{
    tf_string *k = text(key);
    const int32_t value = *(int32_t *)tf_dict_get(d, TF_ELEM_I32, NULL, k);
    tf_str_release(k);
    return value;
}

int main(void)
{
    tf_array d;
    memset(&d, 0, sizeof d);
    EXPECT(tf_dict_count(&d) == 0, "a zeroed Dictionary is empty");

    /* Enough keys to grow the table several times; each keeps its value. */
    char key[16];
    for (int i = 0; i < 1000; ++i)
    {
        snprintf(key, sizeof key, "k%d", i);
        put(&d, key, i * 3);
    }
    EXPECT(tf_dict_count(&d) == 1000, "1000 keys");
    EXPECT(get(&d, "k0") == 0 && get(&d, "k999") == 2997, "values survive growth");
    put(&d, "k5", -1);
    EXPECT(tf_dict_count(&d) == 1000 && get(&d, "k5") == -1, "put replaces");
    tf_string *upper = text("K5");
    EXPECT(!tf_dict_exists(&d, upper), "keys are case-sensitive");
    tf_str_release(upper);

    /* Remove keeps the others in order and findable. */
    tf_string *k1 = text("k1");
    tf_dict_remove(&d, k1);
    tf_str_release(k1);
    EXPECT(tf_dict_count(&d) == 999 && get(&d, "k2") == 6, "remove");
    tf_array keys;
    memset(&keys, 0, sizeof keys);
    tf_dict_keys(&keys, &d);
    EXPECT(keys.count == 999 && tf_array_ubound(&keys, 1) == 998, "Keys has every key");
    EXPECT(strcmp(tf_str_cstr(*(tf_string **)tf_array_at1(&keys, 1)), "k2") == 0,
           "insertion order, the gap closed");
    tf_array items;
    memset(&items, 0, sizeof items);
    tf_dict_items(&items, &d, TF_ELEM_I32, NULL);
    EXPECT(items.count == 999 && *(int32_t *)tf_array_at1(&items, 4) == -1, "Items");

    /* A copy has its own entries. */
    tf_array copy;
    memset(&copy, 0, sizeof copy);
    tf_array_copy(&copy, &d);
    put(&copy, "k0", 77);
    put(&copy, "new", 1);
    EXPECT(get(&d, "k0") == 0 && !tf_dict_exists(&d, NULL), "the original is unchanged");
    EXPECT(get(&copy, "k0") == 77 && tf_dict_count(&copy) == 1000, "the copy changed");

    /* String values, and "" (NULL) as a key. */
    tf_array names;
    memset(&names, 0, sizeof names);
    tf_string *v = text("empty key");
    tf_str_assign((tf_string **)tf_dict_put(&names, TF_ELEM_STRING, NULL, NULL), v);
    EXPECT(tf_dict_exists(&names, NULL) && tf_dict_count(&names) == 1, "\"\" is a key");

    /* Errors inside a Try. */
    tf_try_enter();
    tf_string *missing = text("missing");
    (void)tf_dict_get(&names, TF_ELEM_STRING, NULL, missing);
    EXPECT(tf_error_pending && tf_err_number() == 5, "reading a missing key is error 5");
    tf_err_catch();
    tf_dict_remove(&d, missing);
    EXPECT(tf_error_pending && tf_err_number() == 5, "removing a missing key is error 5");
    tf_err_catch();
    tf_string *k0 = text("k0");
    (void)tf_dict_add(&d, TF_ELEM_I32, NULL, k0);
    EXPECT(tf_error_pending && tf_err_number() == 457, "adding a key twice is error 457");
    tf_err_catch();
    tf_array_lock(&d);
    (void)tf_dict_put(&d, TF_ELEM_I32, NULL, missing);
    EXPECT(tf_error_pending && tf_err_number() == 10, "adding while locked is error 10");
    tf_err_catch();
    *(int32_t *)tf_dict_put(&d, TF_ELEM_I32, NULL, k0) = 5;
    EXPECT(!tf_error_pending && get(&d, "k0") == 5, "replacing while locked is allowed");
    tf_dict_clear(&d);
    EXPECT(tf_error_pending && tf_err_number() == 10, "clearing while locked is error 10");
    tf_err_catch();
    tf_array_unlock(&d);
    tf_try_leave();
    tf_str_release(missing);
    tf_str_release(k0);

    tf_dict_clear(&d);
    EXPECT(tf_dict_count(&d) == 0 && !tf_dict_exists(&d, NULL), "Clear");
    tf_array_free(&d);
    tf_array_free(&keys);
    tf_array_free(&items);
    tf_array_free(&copy);
    tf_array_free(&names);

    if (failures != 0)
    {
        return 1;
    }
    printf("PASS: dictionaries\n");
    return 0;
}
