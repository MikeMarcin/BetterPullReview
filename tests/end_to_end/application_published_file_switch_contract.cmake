cmake_minimum_required(VERSION 4.4.2)

# [[BPR-1326]]. A comment written in Review on a line of
# src/engine/renderer.cpp in the constructed conventional-comments-v1 pull
# request still shows after the reviewer opens another file and comes back:
# its note in the drawer, measured and placed at its line, its gutter marker
# and its scroll-bar tick. Three courses: published once its thread shows
# (settled); published, with the application's own refresh answering only
# once the other file shows (late); and kept as my unpublished draft (draft).
# The draft course failed before BPR-1326: the drawer drew only the selected
# line's draft, and the file shown again selected its first change.

foreach(required IN ITEMS BPR_APPLICATION BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

math(EXPR bpr_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

foreach(course_spec IN ITEMS "settled|11111" "late|10111" "draft|11111")
    string(REPLACE "|" ";" course_parts "${course_spec}")
    list(GET course_parts 0 course)
    list(GET course_parts 1 evidence)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -E
            env
            "QT_QPA_PLATFORM=offscreen"
            "${BPR_APPLICATION}"
            --github-fixture
            conventional-comments-v1
            --published-file-switch-probe
            --published-file-switch-timing
            "${course}"
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
        TIMEOUT ${bpr_timeout}
    )
    if(NOT probe_result EQUAL 0)
        message(FATAL_ERROR
            "published file switch ${course} process failed (${probe_result})\n"
            "stdout:\n${probe_output}\n"
            "stderr:\n${probe_error}"
        )
    endif()
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        PUBLISHED_FILE_SWITCH_OK
        PUBLISHED_FILE_SWITCH_EVIDENCE:${evidence}
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "published file switch ${course} process omitted ${marker}\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
endforeach()

message(STATUS "PUBLISHED_FILE_SWITCH_CONTRACT_OK")
