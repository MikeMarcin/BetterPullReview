cmake_minimum_required(VERSION 4.4.2)

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS BPR_SOURCE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(READ "${BPR_SOURCE}" bpr_application_source)
foreach(forbidden IN ITEMS
    "Implementation->ReviewLedger->"
    "implementation->ReviewLedger->"
    "ReviewLedger->load("
    "ReviewLedger->commit("
)
    string(FIND "${bpr_application_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "review-ledger fixture access bypasses the composed host: ${forbidden}"
        )
    endif()
endforeach()
foreach(required_wire IN ITEMS
    "exercise_review_ledger_composition("
    "seen_action()->trigger()"
    "ledger->persist("
    "ledger->private_observation()"
)
    string(FIND "${bpr_application_source}" "${required_wire}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "review-ledger composition trace omitted ${required_wire}"
        )
    endif()
endforeach()

math(EXPR bpr_phase_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

function(bpr_prepare_private_root root)
    file(REMOVE_RECURSE "${root}")
    file(MAKE_DIRECTORY "${root}")
    file(CHMOD "${root}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
endfunction()

function(bpr_run_review_ledger_probe course epoch_phase root expected_evidence)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -E
            env
            "QT_QPA_PLATFORM=offscreen"
            "${BPR_APPLICATION}"
            --github-fixture
            github-com
            --epoch-fixture-root
            "${root}"
            --epoch-fixture-phase
            "${epoch_phase}"
            --review-ledger-composition-probe
            "${course}"
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
        TIMEOUT ${bpr_phase_timeout}
    )
    if(NOT probe_result EQUAL 0)
        message(FATAL_ERROR
            "review-ledger ${course} process failed (${probe_result})\n"
            "stdout:\n${probe_output}\n"
            "stderr:\n${probe_error}"
        )
    endif()
    string(TOUPPER "${course}" course_marker)
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        GITHUB_FIXTURE_REVIEW_LEDGER_OK:github.com
        REVIEW_LEDGER_COMPOSITION_${course_marker}_OK
        REVIEW_LEDGER_COMPOSITION_EVIDENCE:${expected_evidence}
        REVIEW_LEDGER_OK
        WORKBENCH_HOST_COMPOSED_OK
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "review-ledger ${course} process omitted ${marker}\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
    string(FIND "${probe_output}" "REVIEW_LEDGER_COMPOSITION_${course_marker}_FAILED" failed)
    if(NOT failed EQUAL -1)
        message(FATAL_ERROR
            "review-ledger ${course} process reported failure\n"
            "stdout:\n${probe_output}"
        )
    endif()
endfunction()

bpr_prepare_private_root("${BPR_ROOT}")
bpr_run_review_ledger_probe(seed seed "${BPR_ROOT}" 11111101111101)
bpr_run_review_ledger_probe(reopen reuse "${BPR_ROOT}" 11111111111101)

set(bpr_unavailable_root "${BPR_ROOT}-unavailable")
bpr_prepare_private_root("${bpr_unavailable_root}")
file(MAKE_DIRECTORY
    "${bpr_unavailable_root}/state"
    "${bpr_unavailable_root}/state/v1"
    "${bpr_unavailable_root}/state/v1/review-ledger.sqlite3"
)
file(CHMOD
    "${bpr_unavailable_root}/state"
    "${bpr_unavailable_root}/state/v1"
    "${bpr_unavailable_root}/state/v1/review-ledger.sqlite3"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
)
bpr_run_review_ledger_probe(
    unavailable
    single
    "${bpr_unavailable_root}"
    11111101111111
)
if(NOT IS_DIRECTORY
    "${bpr_unavailable_root}/state/v1/review-ledger.sqlite3"
)
    message(FATAL_ERROR "the forced review-ledger obstruction was replaced")
endif()

file(REMOVE_RECURSE "${BPR_ROOT}" "${bpr_unavailable_root}")
message("REVIEW_LEDGER_COMPOSITION_CONTRACT_OK")
