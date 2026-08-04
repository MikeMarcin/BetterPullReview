cmake_minimum_required(VERSION 4.4.2)

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

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
            ${arguments}
        RESULT_VARIABLE phase_result
        OUTPUT_VARIABLE phase_output
        ERROR_VARIABLE phase_error
        TIMEOUT 240
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

# The seed phase composes the ledger for the first time in this root, so the
# receipt cannot already be there. Asserting the 0 is what makes the 1 on the
# next phase mean something: a probe that always reported "present" would
# otherwise pass.
run_phase(
    "EPOCH_FIXTURE_SEED_OK;REVIEW_LEDGER_PRESENT_ON_ENTRY:0;REVIEW_LEDGER_OK"
    --epoch-fixture-phase
    seed
)
# A separate execution of the real binary, against the same application data
# root, finds the receipt the earlier process committed. This is the restart
# claim: a new process, not a reopened handle.
run_phase(
    "EPOCH_MAINTENANCE_PRUNE_OK;REVIEW_LEDGER_PRESENT_ON_ENTRY:1"
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
