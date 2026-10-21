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
# [[BPR-PENDING-AB]]: three right lines dragged with the mouse, a right-click
# inside them and the menu's "Add comment on lines a–b", each delivered to the
# window as the platform delivers it, open the drawer draft on all three; its
# Publish sends start_line and line, and the thread the conversations then
# show is on all three lines (the seventh and eighth evidence digits).
# [[BPR-1212]] v002: once the epoch host answers that the file's base is the
# merge base's version, Return on a base line in a hunk opens its draft on the
# left, and its Publish sends side LEFT with the base line and no range.
# [[BPR-1570]]: before the base line, a refresh that keeps the file adopts it
# under a new workbench authority with no new publication; the numbering holds
# across it (the second evidence digit) and the base line still takes its
# draft. Before BPR-1570 that order left the base line with no draft.

foreach(required IN ITEMS BPR_APPLICATION BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

math(EXPR bpr_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

# [[BPR-1212]]. The second run turns the first base-numbering answer into
# `cancelled`, as an accept that keeps the comparison leaves it. The course
# must still learn the numbering and publish the base line.
# [[BPR-1570]]. The third makes the kept answer one about another question
# after the refresh, the window's left side still placed: the base line's own
# question is answered at once and must still take a draft.
foreach(bpr_variant IN ITEMS plain cancel-first-base-numbering stale-base-numbering)
    set(bpr_variant_arguments)
    if(bpr_variant STREQUAL "cancel-first-base-numbering")
        set(bpr_variant_arguments --cancel-first-base-numbering)
    elseif(bpr_variant STREQUAL "stale-base-numbering")
        set(bpr_variant_arguments --stale-base-numbering)
    endif()
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
            ${bpr_variant_arguments}
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
        TIMEOUT ${bpr_timeout}
    )
    if(NOT probe_result EQUAL 0)
        message(FATAL_ERROR
            "line draft process failed (${bpr_variant}: ${probe_result})\n"
            "stdout:\n${probe_output}\n"
            "stderr:\n${probe_error}"
        )
    endif()
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        LABEL_AUTHORING_OK
        LINE_DRAFT_EVIDENCE:11111111
        BASE_LINE_DRAFT_EVIDENCE:1111
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "line draft process omitted ${marker} (${bpr_variant})\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
endforeach()

message(STATUS "LINE_DRAFT_CONTRACT_OK")
