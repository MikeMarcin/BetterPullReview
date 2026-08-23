# Narrows directories a contract script pre-creates under a store root to
# the owner-private shape the stores demand.
#
# On macOS and Linux that is mode 0700, which `file(CHMOD … OWNER_READ
# OWNER_WRITE OWNER_EXECUTE)` expresses. On Windows `file(CHMOD)` only
# toggles the read-only attribute, and the catalog refuses a `state` or
# `state/v1` it did not write unless each carries the descriptor it would
# have written itself: a protected DACL whose single entry grants the token
# user full access and inherits to children (BPR-0436). `icacls` writes
# exactly that, and it is the same descriptor the contract reads back on the
# root the application narrows.
function(bpr_narrow_owner_private)
    file(
        CHMOD
        ${ARGN}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
    )
    if(NOT WIN32)
        return()
    endif()
    foreach(directory IN LISTS ARGN)
        file(TO_NATIVE_PATH "${directory}" native_directory)
        execute_process(
            COMMAND
                icacls
                "${native_directory}"
                /inheritance:r
                /grant:r
                "$ENV{USERDOMAIN}\\$ENV{USERNAME}:(OI)(CI)F"
            RESULT_VARIABLE narrow_result
            OUTPUT_QUIET
            ERROR_VARIABLE narrow_error
        )
        if(NOT narrow_result EQUAL 0)
            message(FATAL_ERROR
                "could not narrow ${native_directory}: ${narrow_error}"
            )
        endif()
    endforeach()
endfunction()
