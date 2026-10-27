cmake_minimum_required(VERSION 4.4.2)

# [[BPR-1774]]. The pull request's author edits its description in Overview
# over the public-path fixture, through the shipped composition. After the
# threaded-reply course, Overview offers Edit (the fixture names the viewer
# the author); Save performs exactly one write and the next read shows
# GitHub's body; a second edit meets a body another client changed, and is
# refused with nothing written, the typed text kept and GitHub's newer text
# offered to edit from. Run on github.com and GHES 3.18 fixtures.

foreach(required IN ITEMS BPR_APPLICATION BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

math(EXPR bpr_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

foreach(fixture_spec IN ITEMS "github-com|github.com" "ghes-3.18|ghes-3.18")
    string(REPLACE "|" ";" fixture_parts "${fixture_spec}")
    list(GET fixture_parts 0 fixture)
    list(GET fixture_parts 1 fixture_label)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -E
            env
            "QT_QPA_PLATFORM=offscreen"
            "${BPR_APPLICATION}"
            --github-fixture
            "${fixture}"
            --threaded-reply-probe
            --description-edit-probe
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
        TIMEOUT ${bpr_timeout}
    )
    if(NOT probe_result EQUAL 0)
        message(FATAL_ERROR
            "description edit ${fixture} process failed (${probe_result})\n"
            "stdout:\n${probe_output}\n"
            "stderr:\n${probe_error}"
        )
    endif()
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        GITHUB_FIXTURE_OK:${fixture_label}
        THREADED_REPLY_OK
        DESCRIPTION_EDIT_OK
        DESCRIPTION_EDIT_EVIDENCE:11111
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "description edit ${fixture} process omitted ${marker}\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
endforeach()

message(STATUS "DESCRIPTION_EDIT_CONTRACT_OK")
