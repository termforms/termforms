/* SPDX-License-Identifier: 0BSD */
/*
 * tf_version.h -- the runtime's version.
 *
 * Kept in a header (not passed by the build) because tfc compiles the runtime
 * sources directly with each program. The runtime.version test fails if this
 * drifts from the project version in the top-level CMakeLists.txt.
 */
#ifndef TERMFORMS_TF_VERSION_H
#define TERMFORMS_TF_VERSION_H

#define TF_RUNTIME_VERSION "0.9.2"

#endif /* TERMFORMS_TF_VERSION_H */
