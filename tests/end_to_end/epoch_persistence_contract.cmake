cmake_minimum_required(VERSION 3.28)

foreach(required IN ITEMS BPR_APPLICATION BPR_FIXTURE BPR_FIXTURE_LABEL BPR_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

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
        RESULT_VARIABLE phase_result
        OUTPUT_VARIABLE phase_output
        ERROR_VARIABLE phase_error
        TIMEOUT 180
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
