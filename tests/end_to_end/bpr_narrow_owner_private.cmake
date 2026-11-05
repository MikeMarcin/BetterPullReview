# Narrows directories a contract script pre-creates under a store root to
# the owner-private shape the stores demand.
#
# On macOS and Linux that is mode 0700, which `file(CHMOD … OWNER_READ
# OWNER_WRITE OWNER_EXECUTE)` expresses. On Windows `file(CHMOD)` only
# toggles the read-only attribute, and the catalog refuses a `state` or
# `state/v1` it did not write unless each carries the descriptor it would
# have written itself: a protected DACL whose single entry grants the token
# user full access and inherits to children (BPR-0436). `icacls` writes
# that here. That the product's own `workspace_parent_trusted` accepts it, as it
# accepts the product's descriptor and the Python helper's, is not asserted
# here: the Windows row `owner_private_descriptor_agreement` pins it, and fails
# if this helper or the Python one stops removing a shared inherited grant
# (BPR-0900).
#
# The grant names the token user's SID, not USERDOMAIN\USERNAME ([[BPR-1870]]):
# an OpenSSH logon on a workgroup machine has USERDOMAIN=WORKGROUP, which
# names no account, and icacls refused the grant with 1332 ([[BPR-1869]]).
function(bpr_narrow_owner_private)
    file(
        CHMOD
        ${ARGN}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
    )
    if(NOT WIN32)
        return()
    endif()
    _bpr_current_user_sid(user_sid)
    foreach(directory IN LISTS ARGN)
        file(TO_NATIVE_PATH "${directory}" native_directory)
        execute_process(
            COMMAND
                icacls
                "${native_directory}"
                /inheritance:r
                /grant:r
                "*${user_sid}:(OI)(CI)F"
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

# Windows only: the current user's string SID, from System32's whoami, read
# once per script. whoami is named by path because Git for Windows' usr\bin,
# on PATH for perl, carries a POSIX whoami that has no /user.
function(_bpr_current_user_sid output_variable)
    get_property(sid GLOBAL PROPERTY BPR_CURRENT_USER_SID)
    if(sid STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{SystemRoot}/System32/whoami.exe" whoami)
        execute_process(
            COMMAND "${whoami}" /user /fo csv /nh
            RESULT_VARIABLE whoami_result
            OUTPUT_VARIABLE whoami_output
            ERROR_VARIABLE whoami_error
        )
        if(NOT whoami_result EQUAL 0)
            message(FATAL_ERROR
                "could not read the current user's SID: ${whoami_error}"
            )
        endif()
        if(NOT whoami_output MATCHES "\"(S-1-[0-9-]+)\"")
            message(FATAL_ERROR
                "could not read the current user's SID from: ${whoami_output}"
            )
        endif()
        set(sid "${CMAKE_MATCH_1}")
        set_property(GLOBAL PROPERTY BPR_CURRENT_USER_SID "${sid}")
    endif()
    set(${output_variable} "${sid}" PARENT_SCOPE)
endfunction()

# Gives a course's application data root a parent this platform trusts, and
# answers with the root to use.
#
# [[BPR-0875]]. The application puts its agent workspace neighbourhood beside
# its data root, and `workspace_parent_trusted` refuses a parent another
# principal can write. A course rooted directly in the build tree hands it
# `tests/end_to_end`, which inherits `Authenticated Users:(M)` on Windows, so
# recovery refuses with `repair_required` at startup and the profile reports it
# as a cleanup failure at the quit. The build directory is shared and must stay
# writable, so the course gains one owned level instead: the directory it was
# given becomes the private parent, and the root moves inside it.
#
# Pass the name of the variable holding the root. It is rewritten in the
# caller's scope.
function(bpr_private_data_root root_variable)
    set(parent "${${root_variable}}")
    file(MAKE_DIRECTORY "${parent}")
    bpr_narrow_owner_private("${parent}")
    set(${root_variable} "${parent}/data" PARENT_SCOPE)
endfunction()
