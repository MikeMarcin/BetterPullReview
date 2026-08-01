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
            -Werror
            $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-Wno-missing-field-initializers>
    )
endfunction()
