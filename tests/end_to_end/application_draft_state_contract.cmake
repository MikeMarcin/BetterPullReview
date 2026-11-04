cmake_minimum_required(VERSION 4.4.2)

# [[BPR-1852]]. The author of a draft publishes it from the app over the
# public-path fixture, through the shipped composition. After the
# threaded-reply course, the title row and Overview say the pull request is a
# draft, and Overview's banner and the Pull Request menu offer Ready for
# review. Ready for review takes the chips away at once, writes exactly once,
# and reads nothing and projects no whole Overview until GitHub's answer has
# settled; Convert to draft is then offered, and a refused conversion is shown
# at once and withdrawn with words. Run on github.com and GHES 3.18 fixtures.

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
            --draft-state-probe
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
        TIMEOUT ${bpr_timeout}
    )
    if(NOT probe_result EQUAL 0)
        message(FATAL_ERROR
            "draft state ${fixture} process failed (${probe_result})\n"
            "stdout:\n${probe_output}\n"
            "stderr:\n${probe_error}"
        )
    endif()
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        GITHUB_FIXTURE_OK:${fixture_label}
        THREADED_REPLY_OK
        DRAFT_STATE_OK
        DRAFT_STATE_EVIDENCE:1111
        DRAFT_STATE_MINIMAL_OK
        "DRAFT_STATE_COST:reads=0 overviewBuilds=0 "
        "refreshInFlight=0 "
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "draft state ${fixture} process omitted ${marker}\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
endforeach()

message(STATUS "DRAFT_STATE_CONTRACT_OK")
