cmake_minimum_required(VERSION 4.4.2)

# [[BPR-1862]]. Delete comment on a draft, in the composed application over
# the constructed conventional-comments-v1 pull request with the private
# store, over one data root: `line` deletes a draft on a line inside GitHub's
# diff, `off-hunk` one on a line outside every hunk, and `agent` an agent
# answer's Draft comment on such a line. Each confirms the delete and finds
# the draft gone from the drawer, the notes and the store, and still gone
# with its line chosen again. `agent` also finds that the turn let the
# deleted draft go, that Draft comment again makes a new draft, and that
# deleting that one from Finish your review, once pending, lets it go too.
# `reopened` restarts over the same root and finds none of them came back.
# Before BPR-1862 the store discarded the draft but the window kept it as the
# line's unpublished note, and the agent turn kept saying Go to draft.

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")

math(EXPR bpr_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")
file(REMOVE_RECURSE "${BPR_ROOT}")
bpr_private_data_root(BPR_ROOT)

foreach(course_spec IN ITEMS
    "line|DRAFT_DELETE_EVIDENCE:11111"
    "off-hunk|DRAFT_DELETE_EVIDENCE:11111"
    "agent|DRAFT_DELETE_EVIDENCE:1111111"
    "reopened|DRAFT_DELETE_EVIDENCE:11111")
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
            --epoch-fixture-root
            "${BPR_ROOT}"
            --epoch-fixture-phase
            single
            --draft-delete-course
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
            "draft delete ${course} process failed (${probe_result})\n"
            "stdout:\n${probe_output}\n"
            "stderr:\n${probe_error}"
        )
    endif()
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        DRAFT_DELETE_OK
        "${evidence}\n"
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "draft delete ${course} process omitted ${marker}\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
endforeach()

message(STATUS "DRAFT_DELETE_CONTRACT_OK")
