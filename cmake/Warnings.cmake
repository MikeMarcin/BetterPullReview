function(bpr_enable_warnings target)
    if(MSVC)
        target_compile_options("${target}" PRIVATE /W4 /permissive- /WX)
        return()
    endif()

    target_compile_options(
        "${target}"
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            # BPR-0534: MSVC's C4456 (a local hiding an earlier local) is an
            # error under /W4 /WX, and the clang hosts used to compile the
            # same code silently until the Windows station built it. Clang's
            # -Wshadow is that class; GCC's plain -Wshadow also covers members
            # and globals, so its local-scope form is the matching one. Clang's
            # -Wshadow alone still skips a lambda local that hides an enclosing
            # local the lambda does not capture, which C4456 and GCC's
            # -Wshadow=local both report, so the clang hosts need the uncaptured
            # form as well to see what the Windows station sees.
            $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-Wshadow>
            $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-Wshadow-uncaptured-local>
            $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-Wshadow=local>
            -Werror
            $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-Wno-missing-field-initializers>
    )
endfunction()
