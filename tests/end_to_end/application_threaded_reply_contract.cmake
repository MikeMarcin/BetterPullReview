cmake_minimum_required(VERSION 4.4.2)

# [[BPR-0657]]. Threaded replies and resolve through the mutation port over
# the public-path fixture: a reviewer presses Reply on a thread's card, the
# draft answers the thread and says so, publishes, the reply lands under its
# thread on the next read, Resolve on the card resolves the thread through
# the port and the thread comes back resolved.
#
# [[BPR-PENDING-AR1]]. With `--reaction-probe` the course goes on: Unresolve
# conversation at the thread's end shows the thread open at once and GitHub's
# answer settles it; the recorded thumbs up is read; pressing its chip shows
# mine at once, the write confirms it and a second press removes it; Heart is
# chosen in the picker by keyboard alone; and a refresh agrees with all of it.

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
            --reaction-probe
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
        TIMEOUT ${bpr_timeout}
    )
    if(NOT probe_result EQUAL 0)
        message(FATAL_ERROR
            "threaded reply ${fixture} process failed (${probe_result})\n"
            "stdout:\n${probe_output}\n"
            "stderr:\n${probe_error}"
        )
    endif()
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        GITHUB_FIXTURE_OK:${fixture_label}
        THREADED_REPLY_OK
        THREADED_REPLY_EVIDENCE:11111
        REACTION_OK
        REACTION_EVIDENCE:11111111
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "threaded reply ${fixture} process omitted ${marker}\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
endforeach()

message(STATUS "THREADED_REPLY_CONTRACT_OK")
