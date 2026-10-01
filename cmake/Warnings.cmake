# [[BPR-1260]]. A designated initializer may leave a field to its default
# member initializer: GCC's -Wmissing-field-initializers is off below, and
# tidy.py passes -Wno-missing-designated-field-initializers to clang-tidy.
# Ubuntu clang 21 reports that case under -Wextra and stopped the build on it. Turn it off where the compiler knows it: an older Apple clang
# would refuse an unknown -Wno- option under -Werror.
include(CheckCXXCompilerFlag)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    check_cxx_compiler_flag(
        -Wmissing-designated-field-initializers
        BPR_CXX_HAS_MISSING_DESIGNATED_FIELD_INITIALIZERS
    )
endif()

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
    if(BPR_CXX_HAS_MISSING_DESIGNATED_FIELD_INITIALIZERS)
        target_compile_options(
            "${target}"
            PRIVATE
                $<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:-Wno-missing-designated-field-initializers>
        )
    endif()
endfunction()
