cmake_minimum_required(VERSION 4.4.2)
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")

# [[BPR-0660]]. The enterprise server a reviewer keeps, over two runs against
# one application data root: the seed run remembers "ghe.kept.example" on the
# Accounts & hosts page and requires the stored server to reach the page; the
# reuse run opens over the same store and requires it both kept and offered as
# a host to sign in on. No token is stored either way: the record is a hostname
# and an account name, and signing in there is still the device flow.

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

math(EXPR bpr_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${BPR_ROOT}")

function(bpr_run_saved_server_phase fixture fixture_label root phase)
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
            --saved-server
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
        TIMEOUT ${bpr_timeout}
    )
    if(NOT probe_result EQUAL 0)
        message(FATAL_ERROR
            "saved server ${fixture} ${phase} process failed (${probe_result})\n"
            "stdout:\n${probe_output}\n"
            "stderr:\n${probe_error}"
        )
    endif()
    if(phase STREQUAL "seed")
        set(phase_marker "SAVED_SERVER_SEEDED:host=ghe.kept.example:kept=1")
    else()
        set(phase_marker "SAVED_SERVER:host=ghe.kept.example:kept=1:offered=1")
    endif()
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        GITHUB_FIXTURE_OK:${fixture_label}
        "${phase_marker}"
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "saved server ${fixture} ${phase} process omitted ${marker}\n"
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
    bpr_run_saved_server_phase("${fixture}" "${fixture_label}" "${fixture_root}" seed)
    bpr_run_saved_server_phase("${fixture}" "${fixture_label}" "${fixture_root}" reuse)
endforeach()

message(STATUS "SAVED_SERVER_CONTRACT_OK")
