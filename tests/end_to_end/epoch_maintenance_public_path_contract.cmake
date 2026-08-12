cmake_minimum_required(VERSION 4.4.2)

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

# A backstop, not the bound. The phase bounds itself at `BPR_RUN_BUDGET_MS` and
# reports what it was doing; this only catches a process that never reached its
# own limit, and is derived from that limit so it can never fire first.
math(EXPR bpr_phase_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${BPR_ROOT}")

function(run_phase label)
    set(arguments ${ARGN})
    execute_process(
        COMMAND
            "${BPR_APPLICATION}"
            --github-fixture
            github-com
            --epoch-fixture-root
            "${BPR_ROOT}"
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
            ${arguments}
        RESULT_VARIABLE phase_result
        OUTPUT_VARIABLE phase_output
        ERROR_VARIABLE phase_error
        TIMEOUT ${bpr_phase_timeout}
    )
    if(NOT phase_result EQUAL 0)
        message(FATAL_ERROR
            "${label} process failed (${phase_result})\n"
            "stdout:\n${phase_output}\n"
            "stderr:\n${phase_error}")
    endif()
    foreach(expected IN ITEMS "PUBLIC_PATH_OK" "GITHUB_FIXTURE_OK:github.com")
        string(FIND "${phase_output}" "${expected}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "${label} process omitted ${expected}\n"
                "stdout:\n${phase_output}\n"
                "stderr:\n${phase_error}")
        endif()
    endforeach()
    foreach(expected IN LISTS label)
        string(FIND "${phase_output}" "${expected}" marker)
        if(marker EQUAL -1)
            message(FATAL_ERROR
                "process omitted ${expected}\n"
                "stdout:\n${phase_output}\n"
                "stderr:\n${phase_error}")
        endif()
    endforeach()
endfunction()

# Seed and every maintenance phase use this one fixture root, so the epoch
# catalog and review ledger persist across the four real process executions.
run_phase(
    "EPOCH_FIXTURE_SEED_OK;REVIEW_LEDGER_OK"
    --epoch-fixture-phase
    seed
)
# The prune branch starts maintenance before its selected-material ledger
# probe. Receipt-load persistence belongs to the dedicated review-ledger
# composition contract, not this maintenance verdict.
run_phase(
    "EPOCH_MAINTENANCE_PRUNE_OK"
    --epoch-fixture-phase
    reuse
    --epoch-maintenance-probe
    prune
)
run_phase(
    "EPOCH_MAINTENANCE_PRUNED_RESTART_DELETE_OK"
    --epoch-fixture-phase
    reuse
    --epoch-maintenance-probe
    verify-pruned-delete
)
run_phase(
    "EPOCH_MAINTENANCE_DELETED_RESTART_OK"
    --epoch-fixture-phase
    reuse
    --epoch-maintenance-probe
    verify-deleted
)

file(REMOVE_RECURSE "${BPR_ROOT}")
message("EPOCH_MAINTENANCE_PUBLIC_PATH_CONTRACT_OK")
