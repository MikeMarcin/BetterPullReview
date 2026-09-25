include_guard(GLOBAL)

# Keep each application's elapsed measurement in CTest's successful log, and
# name a clock expiry before any subject-specific failure. This accepts CMake's
# execute_process result vocabulary as well as an ordinary numeric exit code.
function(bpr_report_automated_run context result output error)
    if(output MATCHES "PUBLIC_PATH_BUDGET:[^\r\n]*")
        message(STATUS "${context}: ${CMAKE_MATCH_0}")
    endif()
    if(output MATCHES "RUN_BUDGET_EXPIRED:[^\r\n]*")
        message(FATAL_ERROR "${context}: ${CMAKE_MATCH_0}\nstdout:\n${output}\nstderr:\n${error}")
    endif()
    if(NOT "${result}" STREQUAL "0")
        if("${result}" MATCHES "[Tt]imeout")
            set(headline "${context}: PROCESS_TIMEOUT (${result})")
        else()
            set(headline "${context}: process failed (${result})")
        endif()
        message(FATAL_ERROR "${headline}\nstdout:\n${output}\nstderr:\n${error}")
    endif()
endfunction()
