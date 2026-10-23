/* SPDX-License-Identifier: 0BSD */
/*
 * tf_version.c -- runtime version reporting.
 */
#include "termforms/tf_version.h"

#include "termforms/tf_runtime.h"

const char *tf_runtime_version(void)
{
    return TF_RUNTIME_VERSION;
}
