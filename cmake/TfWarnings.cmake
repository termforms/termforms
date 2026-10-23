# SPDX-License-Identifier: BSD-2-Clause
#
# tf_set_warnings(<target>)
#
# Applies the project's warning policy to a target. The same policy is used for
# the compiler (C++) and the runtime (C) so that code moving between them, or
# into generated C, is held to one standard. TF_WARNINGS_AS_ERRORS turns the
# warnings into errors; CI always enables it.

function(tf_set_warnings target)
    if(MSVC)
        # /W4: high warning level; /permissive-: standards conformance;
        # /utf-8: source and execution character sets are UTF-8 on every platform.
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        # getenv, sscanf and friends are standard C; MSVC's "use _s" advice is not portable.
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
        if(TF_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wformat=2
            -Wundef)
        if(TF_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
