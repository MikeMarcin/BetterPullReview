cmake_minimum_required(VERSION 4.4.2)

# [[BPR-1331]]. What the private progress button shows when the pull request
# gets a push. On the first head of conventional-comments-v1 the course
# accepts src/engine/renderer.cpp and src/main.cpp and excludes
# src/ui/widgets/button.cpp. The application then restarts over the same data
# root and is served the next push, which changes only renderer.cpp. Under
# RFC-BPR-0007 a receipt survives exactly when its material is unchanged:
# renderer.cpp loses its acceptance; explicitly opening its new material then
# records Seen under BPR-1506, main.cpp
# stays accepted, and button.cpp stays excluded. Before BPR-1331 the receipt
# was keyed by the comparison digest, which names the epoch, so main.cpp
# reopened unread too.

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")

math(EXPR bpr_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")
file(REMOVE_RECURSE "${BPR_ROOT}")
bpr_private_data_root(BPR_ROOT)

set(bpr_first_head "b20cb443428212543f5911e091a0ffeb407d23cf")
set(bpr_pushed_head "cd24836cfb58a9c14f4d3464d68a57ff8aa2a186")
set(bpr_before_marks "head=${bpr_first_head};src/engine/renderer.cpp=Private progress: accepted;src/main.cpp=Private progress: accepted;src/ui/widgets/button.cpp=Private progress: excluded")
set(bpr_after_marks "head=${bpr_pushed_head};src/engine/renderer.cpp=Private progress: seen;src/main.cpp=Private progress: accepted;src/ui/widgets/button.cpp=Private progress: excluded")

foreach(course_spec IN ITEMS "before|single" "after|reuse")
    string(REPLACE "|" ";" course_parts "${course_spec}")
    list(GET course_parts 0 course)
    list(GET course_parts 1 phase)
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
            "${phase}"
            --review-progress-push-probe
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
            "review progress push ${course} process failed (${probe_result})\n"
            "stdout:\n${probe_output}\n"
            "stderr:\n${probe_error}"
        )
    endif()
    if(course STREQUAL "before")
        set(expected_marks "${bpr_before_marks}")
    else()
        set(expected_marks "${bpr_after_marks}")
    endif()
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        REVIEW_PROGRESS_PUSH_OK
        "REVIEW_PROGRESS_PUSH_MARKS:${expected_marks}\n"
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "review progress push ${course} process omitted ${marker}\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
endforeach()

file(REMOVE_RECURSE "${BPR_ROOT}")
message(STATUS "REVIEW_PROGRESS_PUSH_CONTRACT_OK")
