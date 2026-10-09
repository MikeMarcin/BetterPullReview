cmake_minimum_required(VERSION 4.4.2)

# [[BPR-1212]]. The keyboard line cursor and Return in the code, in the
# composed application over the constructed conventional-comments-v1 pull
# request, signed in with the fixture's account, before any publication:
# Return on the base's first line, outside every hunk, opens nothing and the
# status line says why; Return on one free right line opens the existing
# composer on its one-line draft;
# Shift+Down twice selects three right lines in one hunk and Shift+Return
# opens the composer on their draft, prefilled with their exact text in a
# suggestion block with the caret at its last line; Publish sends GitHub's
# start_line, line, start_side and side to the fixture server; with no line
# selected, Return opens nothing and the status line says how to select one.
# [[BPR-1212]] v002: once the epoch host answers that the file's base is the
# merge base's version, Return on a base line in a hunk opens its draft on the
# left, and its Publish sends side LEFT with the base line and no range.

foreach(required IN ITEMS BPR_APPLICATION BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

math(EXPR bpr_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -E
        env
        "QT_QPA_PLATFORM=offscreen"
        "${BPR_APPLICATION}"
        --github-fixture
        conventional-comments-v1
        --label-authoring-probe
        --line-draft-course
        --automated-run-budget-ms
        "${BPR_RUN_BUDGET_MS}"
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_output
    ERROR_VARIABLE probe_error
    TIMEOUT ${bpr_timeout}
)
if(NOT probe_result EQUAL 0)
    message(FATAL_ERROR
        "line draft process failed (${probe_result})\n"
        "stdout:\n${probe_output}\n"
        "stderr:\n${probe_error}"
    )
endif()
foreach(marker IN ITEMS
    PUBLIC_PATH_OK
    LABEL_AUTHORING_OK
    LINE_DRAFT_EVIDENCE:111111
    BASE_LINE_DRAFT_EVIDENCE:111
)
    string(FIND "${probe_output}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "line draft process omitted ${marker}\n"
            "stdout:\n${probe_output}"
        )
    endif()
endforeach()

message(STATUS "LINE_DRAFT_CONTRACT_OK")
