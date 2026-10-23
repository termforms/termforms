/* SPDX-License-Identifier: 0BSD */
/*
 * tf_version.c -- runtime version reporting.
 */
#include "termforms/tf_runtime.h"

/* Supplied by the build (runtime/CMakeLists.txt); guard against a bare compile. */
#ifndef TF_RUNTIME_VERSION
#error "TF_RUNTIME_VERSION must be defined by the build"
#endif

const char *tf_runtime_version(void)
{
    return TF_RUNTIME_VERSION;
}
