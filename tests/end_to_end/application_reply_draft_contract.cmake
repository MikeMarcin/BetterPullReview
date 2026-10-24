cmake_minimum_required(VERSION 4.4.2)

# [[BPR-1374]]. An unsent reply, in the composed application over the
# constructed conventional-comments-v1 pull request, on the first thread of
# src/engine/renderer.cpp (PRRT_cc_1, line 6), written as a reviewer does:
# the thread's Reply, then typing in the reply in place.
#
# refresh   Refresh with no change on the server: the editor holds the same
#           reply again, its thread's note is marked, and what is typed after
#           the Refresh goes into the same draft.
# edited    Another client edits the thread's root, then Refresh: the edited
#           root shows, and the reply is held and marked as before.
# write     The reply is written and the application quits; then restart,
#           over the same data root: the saved reply is marked on its thread's
#           note, and Reply opens it with its words.
# deleted   Another client deletes the thread's root, then Refresh: the thread
#           is gone, and the reply is kept as a note at its line that says
#           "The thread this replies to was deleted on GitHub." Copy text
#           copies its words; Discard asks, then discards it.
# kept      As deleted, then Keep as comment: the words are a new comment on
#           line 6, still unpublished.
# resolved  Another client resolves the thread, then Refresh: the reply is
#           held, its thread's note says it was resolved, and Publish is still
#           offered.
#
# [[BPR-1745]]. The same thread made outdated by a push first, then its
# Reply in the drawer: a reply bound to the thread, with no refusal and the
# quiet line "Outdated: this thread's line has changed since it was written".
# Before the change its Reply was refused and no draft was made.
#
# outdated-publish  The reply is written and published: exactly one reply
#                   reaches the fixture server, bound to the thread and the
#                   commit the thread was written on, and it lands in the
#                   thread.
# outdated-write    The reply is written and the application quits.
# outdated-restart  Restart over the same data root: the saved reply is marked
#                   on its thread's note, and Reply opens it with its words.
#
# Every other course checks that the fixture server received no post, reply,
# review or resolution. Before BPR-1374 every course but write failed: a
# Refresh left the editor with no draft, a restart showed no reply, and a
# deleted thread took its reply with it, unsaid.

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")

math(EXPR bpr_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")
file(REMOVE_RECURSE "${BPR_ROOT}")
bpr_private_data_root(BPR_ROOT)
# [[BPR-1745]]. The outdated-thread pair keeps its own data root, so its
# write is the first run over it, as write is over the other.
set(BPR_OUTDATED_ROOT "${BPR_ROOT}-outdated")
file(REMOVE_RECURSE "${BPR_OUTDATED_ROOT}")
bpr_private_data_root(BPR_OUTDATED_ROOT)

foreach(course_spec IN ITEMS
    "refresh|fresh"
    "edited|fresh"
    "write|persistent"
    "restart|persistent"
    "deleted|fresh"
    "kept|fresh"
    "resolved|fresh"
    "outdated-publish|fresh"
    "outdated-write|outdated"
    "outdated-restart|outdated")
    string(REPLACE "|" ";" course_parts "${course_spec}")
    list(GET course_parts 0 course)
    list(GET course_parts 1 root)
    set(bpr_root_arguments)
    if(root STREQUAL "persistent")
        set(bpr_root_arguments --epoch-fixture-root "${BPR_ROOT}" --epoch-fixture-phase single)
    elseif(root STREQUAL "outdated")
        set(bpr_root_arguments --epoch-fixture-root "${BPR_OUTDATED_ROOT}" --epoch-fixture-phase single)
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
            --reply-draft-course
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
            "reply draft ${course} process failed (${probe_result})\n"
            "stdout:\n${probe_output}\n"
            "stderr:\n${probe_error}"
        )
    endif()
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        REPLY_DRAFT_OK
        "REPLY_DRAFT_EVIDENCE:1111111\n"
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "reply draft ${course} process omitted ${marker}\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
    message(STATUS "reply draft ${course}: passed")
endforeach()

message(STATUS "REPLY_DRAFT_CONTRACT_OK")
