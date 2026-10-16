function(bpr_enable_hardening target)
    set_property(TARGET "${target}" PROPERTY C_EXTENSIONS OFF)
    set_property(TARGET "${target}" PROPERTY CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options("${target}" PRIVATE /sdl /guard:cf)
        # BPR-1027. A /guard:cf image must be linked whole. An incremental relink
        # can point a vtable at a new ILT thunk and leave that thunk out of the
        # Guard CF function table, and the next virtual call through it fails
        # fast (0xC0000409, subcode 0xA) in a correct program. CMake's Debug and
        # RelWithDebInfo defaults link /INCREMENTAL; the later option wins, both
        # in link.exe and in the vs_link_exe wrapper. cfg_complete_link_contract
        # refuses the combination.
        target_link_options("${target}" PRIVATE /guard:cf /INCREMENTAL:NO /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT)
        get_target_property(target_type "${target}" TYPE)
        if(target_type STREQUAL "EXECUTABLE" OR target_type STREQUAL "SHARED_LIBRARY")
            # Keep complete public/static link evidence outside the installed payload.
            target_link_options("${target}" PRIVATE /MAP)
        endif()
        if(target_type STREQUAL "EXECUTABLE")
            # [[BPR-0974]]. A Release image names its program database, as the
            # Debug image and every Qt DLL beside it do: a CodeView record whose
            # path is the bare file name (/PDBALTPATH), so no build path ships,
            # and the database itself stays in the build tree. /DEBUG would
            # otherwise turn off unreferenced-code removal and folding, so
            # /OPT:REF and /OPT:ICF keep the Release layout; the incremental
            # choice above still ends on /INCREMENTAL:NO.
            target_link_options(
                "${target}"
                PRIVATE
                    "$<$<CONFIG:Release>:/DEBUG>"
                    "$<$<CONFIG:Release>:/PDBALTPATH:%_PDB%>"
                    "$<$<CONFIG:Release>:/OPT:REF>"
                    "$<$<CONFIG:Release>:/OPT:ICF>"
            )
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
