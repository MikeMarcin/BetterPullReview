cmake_minimum_required(VERSION 4.4.2)

# [[BPR-0882]]. Every step between a profile's admission and its first window
# reports `startup_failure=unavailable`, and that one word was all a developer
# build said: the startup's catch dropped what it caught. This contract drives
# the shipped entry point into one of those steps for real. With no Git on the
# search path, the profile runtime's constructor throws.
#
# It checks, in order:
#   1. The refusal keeps its existing verdicts: exit 1, stdout exactly the one
#      word, and the typed cause line existing readers match.
#   2. The arm that refused is named on its own line, with the text of what it
#      threw.
#
# Emptying the search path is how the run removes Git. The application looks
# for Git there and nowhere else.

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

# A backstop, not the bound: the application bounds itself at
# `BPR_RUN_BUDGET_MS`, and this only catches a process that never reached it.
math(EXPR bpr_run_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

set(bpr_expected_source
    "profile-runtime-construction: Application data or installed Git is unavailable")

file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${BPR_ROOT}/empty-search-path")

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -E env
        "PATH=${BPR_ROOT}/empty-search-path"
        "${BPR_APPLICATION}"
        --self-test
        --automated-run-budget-ms
        "${BPR_RUN_BUDGET_MS}"
    RESULT_VARIABLE refused_result
    OUTPUT_VARIABLE refused_output
    ERROR_VARIABLE refused_error
    TIMEOUT ${bpr_run_timeout}
)
if(NOT refused_result EQUAL 1 OR NOT refused_output STREQUAL "STARTUP_FAILED\n")
    message(FATAL_ERROR
        "a run with no Git was not refused with the one-word verdict "
        "(${refused_result})\n"
        "stdout:\n${refused_output}\n"
        "stderr:\n${refused_error}")
endif()
foreach(expected IN ITEMS
        "STARTUP_FAILED_CAUSE: startup_failure=unavailable\n"
        "STARTUP_FAILURE_SOURCE:${bpr_expected_source}\n")
    string(FIND "${refused_error}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "the refusal did not say ${expected}"
            "stdout:\n${refused_output}\n"
            "stderr:\n${refused_error}")
    endif()
endforeach()
file(REMOVE_RECURSE "${BPR_ROOT}")

message("STARTUP_FAILURE_SOURCE_OK:profile-runtime-construction")
