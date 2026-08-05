cmake_minimum_required(VERSION 4.4.2)

foreach(required IN ITEMS BPR_APPLICATION BPR_FIXTURE BPR_FIXTURE_LABEL BPR_ROOT BPR_RUN_BUDGET_MS)
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

function(run_epoch_phase phase marker)
    execute_process(
        COMMAND
            "${BPR_APPLICATION}"
            --github-fixture
            "${BPR_FIXTURE}"
            --epoch-fixture-root
            "${BPR_ROOT}"
            --epoch-fixture-phase
            "${phase}"
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
        RESULT_VARIABLE phase_result
        OUTPUT_VARIABLE phase_output
        ERROR_VARIABLE phase_error
        TIMEOUT ${bpr_phase_timeout}
    )
    if(NOT phase_result EQUAL 0)
        message(FATAL_ERROR
            "${phase} process failed (${phase_result})\n"
            "stdout:\n${phase_output}\n"
            "stderr:\n${phase_error}")
    endif()
    foreach(expected IN ITEMS
            "PUBLIC_PATH_OK"
            "${marker}"
            "GITHUB_FIXTURE_OK:${BPR_FIXTURE_LABEL}")
        string(FIND "${phase_output}" "${expected}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "${phase} process omitted ${expected}\n"
                "stdout:\n${phase_output}\n"
                "stderr:\n${phase_error}")
        endif()
    endforeach()
endfunction()

run_epoch_phase(seed "EPOCH_FIXTURE_SEED_OK")
run_epoch_phase(reuse "EPOCH_FIXTURE_REUSE_OK")
file(REMOVE_RECURSE "${BPR_ROOT}")

message("EPOCH_PERSISTENCE_CONTRACT_OK:${BPR_FIXTURE_LABEL}")
