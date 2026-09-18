cmake_minimum_required(VERSION 4.4.2)
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")

# [[BPR-0654]]. The overview's "changed since my last review" column over the
# fixtures that serve a second head, in the epoch fixture's two phases: the
# seed run opens the first head and marks it reviewed; the reuse run opens
# the second head over the retained cache, and the overview must name
# exactly the one file whose blob differs between the two heads, count it,
# and land Continue review on it.

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

math(EXPR bpr_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

# [[BPR-0875]]. The data root moves inside an owned parent, because the build
# tree this course roots in is writable by other principals and the agent
# workspace neighbourhood refuses such a parent.
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")
bpr_private_data_root(BPR_ROOT)

file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${BPR_ROOT}")

function(bpr_run_overview_phase fixture fixture_label root phase)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -E
            env
            "QT_QPA_PLATFORM=offscreen"
            "${BPR_APPLICATION}"
            --github-fixture
            "${fixture}"
            --epoch-fixture-root
            "${root}"
            --epoch-fixture-phase
            "${phase}"
            --overview-since-review
            --render-evidence
            "${root}/overview-${phase}.png"
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
        TIMEOUT ${bpr_timeout}
    )
    if(NOT probe_result EQUAL 0)
        message(FATAL_ERROR
            "overview since-review ${fixture} ${phase} process failed (${probe_result})\n"
            "stdout:\n${probe_output}\n"
            "stderr:\n${probe_error}"
        )
    endif()
    if(phase STREQUAL "seed")
        set(phase_marker "OVERVIEW_SINCE_REVIEW_SEEDED")
    else()
        set(phase_marker "OVERVIEW_SINCE_REVIEW:files=1:changed=review.cpp:entry=review.cpp")
    endif()
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        GITHUB_FIXTURE_OK:${fixture_label}
        "${phase_marker}"
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "overview since-review ${fixture} ${phase} process omitted ${marker}\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
endfunction()

foreach(fixture_spec IN ITEMS "github-com|github.com" "ghes-3.18|ghes-3.18")
    string(REPLACE "|" ";" fixture_parts "${fixture_spec}")
    list(GET fixture_parts 0 fixture)
    list(GET fixture_parts 1 fixture_label)
    set(fixture_root "${BPR_ROOT}/${fixture}")
    file(MAKE_DIRECTORY "${fixture_root}")
    # The registry refuses a pre-created root it did not narrow itself (BPR-0723).
    bpr_narrow_owner_private("${fixture_root}")
    bpr_run_overview_phase("${fixture}" "${fixture_label}" "${fixture_root}" seed)
    bpr_run_overview_phase("${fixture}" "${fixture_label}" "${fixture_root}" reuse)
endforeach()

message(STATUS "OVERVIEW_SINCE_REVIEW_CONTRACT_OK")
