/* SPDX-License-Identifier: 0BSD */
/*
 * tf_runtime.h -- public interface of the TermForms runtime library (tfrt).
 *
 * Every program produced by the TermForms compiler includes this header and
 * links against tfrt. The interface is plain C99 so that generated code, the
 * runtime and any C library a program declares can all be compiled by the
 * same C compiler.
 *
 * Naming: every public identifier starts with tf_ (functions, types) or TF_
 * (macros), so generated code and user C libraries cannot collide with it.
 */
#ifndef TERMFORMS_TF_RUNTIME_H
#define TERMFORMS_TF_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Returns the runtime's version as a "major.minor.patch" string.
 * The string is static and must not be freed.
 */
const char *tf_runtime_version(void);

#ifdef __cplusplus
}
#endif

#endif /* TERMFORMS_TF_RUNTIME_H */
