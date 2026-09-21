function(bpr_enable_hardening target)
    set_property(TARGET "${target}" PROPERTY C_EXTENSIONS OFF)
    set_property(TARGET "${target}" PROPERTY CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options("${target}" PRIVATE /sdl /guard:cf)
        target_link_options("${target}" PRIVATE /guard:cf /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT)
        get_target_property(target_type "${target}" TYPE)
        if(target_type STREQUAL "EXECUTABLE")
            # Keep complete public/static link evidence outside the installed payload.
            target_link_options("${target}" PRIVATE /MAP)
        endif()
        return()
    endif()

    target_compile_options(
        "${target}"
        PRIVATE
            -fstack-protector-strong
            -fno-omit-frame-pointer
            "$<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=3>"
    )

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options("${target}" PRIVATE -Wl,-z,relro,-z,now)
    endif()
endfunction()
