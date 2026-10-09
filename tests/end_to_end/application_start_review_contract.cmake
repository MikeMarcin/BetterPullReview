cmake_minimum_required(VERSION 4.4.2)

# [[BPR-1345]]. Start a review, in the composed application over the
# constructed conventional-comments-v1 pull request, on
# src/engine/renderer.cpp, through the production review session and its
# journal against the fixture server:
#
# pending   A drawer draft offers Add single comment and Start a review;
#           Start a review keeps it pending with nothing sent. The next
#           draft's main action is Add review comment, and ⌘↩ adds a range to
#           the review; the bar says "Review · 2 pending" and both are drawn at
#           their lines. Add single comment on a third publishes exactly that
#           one; another file and back changes nothing.
# finish    Restarted over the same data root: both are pending again.
#           Finish review opens the summary in Discuss listing both; Comment
#           with no summary is submitted as one review whose comments are
#           exactly the two (path, lines, side, words), and each is marked
#           published on GitHub's acceptance.
# refused   The server refuses the review (422): the draft stays pending with
#           its text; Discard review takes it out and keeps it.
# uncertain The answer and the review list are lost: the draft is held with
#           its text and no retry; Check status, once the list reads again,
#           finds the review and marks it published, with one review sent.

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
    "pending|persistent|START_REVIEW_PENDING_EVIDENCE:11111"
    "finish|persistent|START_REVIEW_FINISH_EVIDENCE:11111"
    "refused|fresh|START_REVIEW_REFUSED_EVIDENCE:1111"
    "uncertain|fresh|START_REVIEW_UNCERTAIN_EVIDENCE:1111")
    string(REPLACE "|" ";" course_parts "${course_spec}")
    list(GET course_parts 0 course)
    list(GET course_parts 1 root)
    list(GET course_parts 2 evidence)
    set(bpr_root_arguments)
    if(root STREQUAL "persistent")
        set(bpr_root_arguments --epoch-fixture-root "${BPR_ROOT}" --epoch-fixture-phase single)
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
            ${bpr_root_arguments}
            --start-review-course
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
            "start review ${course} process failed (${probe_result})\n"
            "stdout:\n${probe_output}\n"
            "stderr:\n${probe_error}"
        )
    endif()
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        START_REVIEW_OK
        "${evidence}\n"
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "start review ${course} process omitted ${marker}\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
endforeach()

message(STATUS "START_REVIEW_CONTRACT_OK")
