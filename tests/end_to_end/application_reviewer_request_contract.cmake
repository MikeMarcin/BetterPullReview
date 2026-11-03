cmake_minimum_required(VERSION 4.4.2)

# [[BPR-1804]]. A review is requested from Overview's People over the
# public-path fixture, through the shipped composition. After the
# threaded-reply course, People offers Request review; the picker lists
# GitHub's suggestion and the people who can review, a typed query narrows it
# to one person, choosing them by keyboard writes exactly once, and choosing
# them again sends nothing. Run on github.com and GHES 3.18 fixtures.
# [[BPR-1849]]. From the choice until GitHub's answers settle, nothing reads
# the pull request or projects the whole Overview: People lists the person at
# once, pending, GitHub's answer confirms them, and a refused request is
# withdrawn with the picker saying why.

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
            --reviewer-request-probe
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
        TIMEOUT ${bpr_timeout}
    )
    if(NOT probe_result EQUAL 0)
        message(FATAL_ERROR
            "reviewer request ${fixture} process failed (${probe_result})\n"
            "stdout:\n${probe_output}\n"
            "stderr:\n${probe_error}"
        )
    endif()
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        GITHUB_FIXTURE_OK:${fixture_label}
        THREADED_REPLY_OK
        REVIEWER_REQUEST_OK
        REVIEWER_REQUEST_EVIDENCE:1111
        REVIEWER_REQUEST_MINIMAL_OK
        "REVIEWER_REQUEST_COST:reads=0 overviewBuilds=0 "
        "refreshInFlight=0 shownAtOnce=1 confirmed=1 rolledBack=1 "
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "reviewer request ${fixture} process omitted ${marker}\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
endforeach()

message(STATUS "REVIEWER_REQUEST_CONTRACT_OK")
