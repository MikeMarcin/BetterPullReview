cmake_minimum_required(VERSION 4.4.2)

# [[BPR-1868]]. Two data roots nothing was wrong with closed the application at
# startup before any window, and both said only
# `STARTUP_FAILED_CAUSE: startup_failure=registry` and
# `STARTUP_PROFILE_REFUSAL:registry_unavailable`:
#
#   - a root typed in another letter case than its folders on a
#     case-insensitive volume (`.../BPR-attended/data` over `.../bpr-attended`),
#     whose canonical spelling then differed from the one given;
#   - a root that already existed before the first launch.
#
# This contract drives the shipped entry point with `--self-test` and
# `--test-application-data-root` and checks, in order:
#   1. A root made beforehand owner-private (mode 0700) starts.
#   2. A root made beforehand with a shared mode (what `mkdir -p` gives) is
#      still refused, because a folder that already existed is never repaired,
#      and the refusal now names that cause on its own line.
#   3. On a case-insensitive volume, a missing root under a folder spelled in
#      another letter case starts, and is created under the folder's own
#      spelling; then the same root spelled in a third case starts again.
#      On a case-sensitive volume the step reports that it did not apply.

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

math(EXPR bpr_run_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

function(run_start label root output_name error_name result_name)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}" -E env
            "QT_QPA_PLATFORM=offscreen"
            "${BPR_APPLICATION}"
            --self-test
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
            --test-application-data-root
            "${root}"
        RESULT_VARIABLE run_result
        OUTPUT_VARIABLE run_output
        ERROR_VARIABLE run_error
        TIMEOUT ${bpr_run_timeout}
    )
    set(${output_name} "${run_output}" PARENT_SCOPE)
    set(${error_name} "${run_error}" PARENT_SCOPE)
    set(${result_name} "${run_result}" PARENT_SCOPE)
endfunction()

function(require_started label root)
    run_start("${label}" "${root}" output error result)
    string(FIND "${output}" "STARTUP_FAILED" failed)
    if(NOT result EQUAL 0 OR NOT failed EQUAL -1)
        message(FATAL_ERROR
            "${label} did not start (${result})\n"
            "stdout:\n${output}\n"
            "stderr:\n${error}")
    endif()
endfunction()

include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")
file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${BPR_ROOT}")
bpr_narrow_owner_private("${BPR_ROOT}")

# 1. Made beforehand, owner-private.
file(MAKE_DIRECTORY "${BPR_ROOT}/premade-private")
bpr_narrow_owner_private("${BPR_ROOT}/premade-private")
require_started("a root made beforehand with mode 0700" "${BPR_ROOT}/premade-private")

# 2. Made beforehand, shared: refused, and the cause is named.
file(MAKE_DIRECTORY "${BPR_ROOT}/premade-shared")
file(CHMOD "${BPR_ROOT}/premade-shared"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
run_start("a root made beforehand with mode 0755" "${BPR_ROOT}/premade-shared" shared_output shared_error shared_result)
if(NOT shared_result EQUAL 1 OR NOT shared_output STREQUAL "STARTUP_FAILED\n")
    message(FATAL_ERROR
        "a shared pre-existing root was not refused with the one-word verdict "
        "(${shared_result})\n"
        "stdout:\n${shared_output}\n"
        "stderr:\n${shared_error}")
endif()
foreach(expected IN ITEMS
        "STARTUP_FAILED_CAUSE: startup_failure=registry\n"
        "STARTUP_FAILURE_SOURCE:registry-root-permissions: ")
    string(FIND "${shared_error}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "the shared root's refusal did not say ${expected}\n"
            "stderr:\n${shared_error}")
    endif()
endforeach()
file(GLOB shared_children "${BPR_ROOT}/premade-shared/*")
if(NOT shared_children STREQUAL "")
    message(FATAL_ERROR "the refused shared root was written to: ${shared_children}")
endif()

# 3. Another letter case, on a case-insensitive volume only.
file(MAKE_DIRECTORY "${BPR_ROOT}/Attended")
if(EXISTS "${BPR_ROOT}/attended")
    require_started("a missing root under a folder spelled in another case" "${BPR_ROOT}/attended/data")
    file(GLOB spelled RELATIVE "${BPR_ROOT}" "${BPR_ROOT}/*ttended")
    file(GLOB created RELATIVE "${BPR_ROOT}/Attended" "${BPR_ROOT}/Attended/data")
    if(NOT spelled STREQUAL "Attended" OR NOT created STREQUAL "data")
        message(FATAL_ERROR
            "the root was not created under the folder's own spelling: "
            "folders '${spelled}', root '${created}'")
    endif()
    require_started("an existing root spelled in a third case" "${BPR_ROOT}/ATTENDED/data")
    set(bpr_case_step "case-insensitive")
else()
    set(bpr_case_step "case-sensitive-not-applicable")
endif()

file(REMOVE_RECURSE "${BPR_ROOT}")
message("STARTUP_DATA_ROOT_OK:${bpr_case_step}")
