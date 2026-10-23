# SPDX-License-Identifier: BSD-2-Clause
#
# When TF_SANITIZE is ON, every target in the build is compiled and linked with
# AddressSanitizer and UndefinedBehaviorSanitizer. LeakSanitizer is part of
# ASan on Linux and is enabled there by default; macOS does not support it, so
# leak checking happens on the Linux runners only.
#
# The flags are applied globally (not per target) because sanitizer runtimes
# must be consistent across everything linked into one executable.

if(TF_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "TF_SANITIZE is supported with GCC and Clang only")
    endif()

    set(_tf_san_flags -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_compile_options(${_tf_san_flags})
    add_link_options(-fsanitize=address,undefined)
    message(STATUS "TermForms: building with AddressSanitizer and UBSan")
endif()
