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
    string(FIND "${phase_output}" "${label}" marker)
    if(marker EQUAL -1)
        message(FATAL_ERROR
            "process omitted ${label}\n"
            "stdout:\n${phase_output}\n"
            "stderr:\n${phase_error}")
    endif()
endfunction()

run_phase(
    "EPOCH_FIXTURE_SEED_OK"
    --epoch-fixture-phase
    seed
)
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
