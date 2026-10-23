/* SPDX-License-Identifier: 0BSD */
/*
 * test_runtime_version.c -- the runtime reports the version it was built as.
 */
#include "termforms/tf_runtime.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *version = tf_runtime_version();

    if (version == NULL)
    {
        fprintf(stderr, "FAIL: tf_runtime_version() returned NULL\n");
        return 1;
    }
    if (strcmp(version, TF_EXPECTED_VERSION) != 0)
    {
        fprintf(stderr, "FAIL: expected version %s, got %s\n", TF_EXPECTED_VERSION, version);
        return 1;
    }

    printf("PASS: runtime version %s\n", version);
    return 0;
}
