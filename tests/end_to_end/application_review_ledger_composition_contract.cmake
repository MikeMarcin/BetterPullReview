cmake_minimum_required(VERSION 4.4.2)
include("${CMAKE_CURRENT_LIST_DIR}/bpr_automated_run_result.cmake")

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS BPR_SINGLE_RUN_BUDGET_MS BPR_SOURCE)
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

# [[BPR-0875]]. `file(CHMOD)` says "only me" on POSIX and only clears the
# read-only attribute on Windows, so the root was never owner-private there
# and the stores refused it. The shared helper writes the descriptor each
# platform actually needs.
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")

function(bpr_prepare_private_root root)
    file(REMOVE_RECURSE "${root}")
    file(MAKE_DIRECTORY "${root}")
    bpr_narrow_owner_private("${root}")
endfunction()

function(bpr_run_review_ledger_probe course epoch_phase root expected_evidence expected_mark)
    set(run_budget_ms "${BPR_RUN_BUDGET_MS}")
    if(epoch_phase STREQUAL "single")
        set(run_budget_ms "${BPR_SINGLE_RUN_BUDGET_MS}")
    endif()
    math(EXPR bpr_phase_timeout "${run_budget_ms} / 1000 + 30")
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
            "${run_budget_ms}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
        TIMEOUT ${bpr_phase_timeout}
    )
    bpr_report_automated_run("review-ledger ${course}"
        "${probe_result}" "${probe_output}" "${probe_error}")
    string(TOUPPER "${course}" course_marker)
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        GITHUB_FIXTURE_REVIEW_LEDGER_OK:github.com
        REVIEW_LEDGER_COMPOSITION_${course_marker}_OK
        REVIEW_LEDGER_COMPOSITION_EVIDENCE:${expected_evidence}
        "${expected_mark}"
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
    if(course STREQUAL "seed")
        string(FIND "${probe_output}" "REVIEW_LEDGER_ACCEPT_SHORTCUT_REPEAT_OK" accept_shortcut)
        if(accept_shortcut EQUAL -1)
            message(FATAL_ERROR "review-ledger seed omitted actual Accept-key/repeat evidence\n${probe_output}")
        endif()
    endif()
    string(FIND "${probe_output}" "REVIEW_LEDGER_COMPOSITION_${course_marker}_FAILED" failed)
    if(NOT failed EQUAL -1)
        message(FATAL_ERROR
            "review-ledger ${course} process reported failure\n"
            "stdout:\n${probe_output}"
        )
    endif()
endfunction()

# [[BPR-0875]]. The data root gains an owned parent: the build directory it
# was given is shared and the stores refuse a parent another principal can
# write.
bpr_private_data_root(BPR_ROOT)
bpr_prepare_private_root("${BPR_ROOT}")
# [[BPR-1331]]. The private progress button says accepted after the gestures,
# and Seen automatically when the changed file is opened on the pushed head.
bpr_run_review_ledger_probe(seed seed "${BPR_ROOT}" 111111011111011
    "REVIEW_LEDGER_PROGRESS_MARK:Private progress: accepted\n")
bpr_run_review_ledger_probe(reopen reuse "${BPR_ROOT}" 111111111111011
    "REVIEW_LEDGER_PROGRESS_MARK:Private progress: seen\n")
# A second process at the same material loads automatic Seen without a write.
bpr_run_review_ledger_probe(reopen reuse "${BPR_ROOT}" 111111111111011
    "REVIEW_LEDGER_PROGRESS_MARK:Private progress: seen\n")

set(bpr_unavailable_root "${BPR_ROOT}-unavailable")
bpr_prepare_private_root("${bpr_unavailable_root}")
file(MAKE_DIRECTORY
    "${bpr_unavailable_root}/state"
    "${bpr_unavailable_root}/state/v1"
    "${bpr_unavailable_root}/state/v1/review-ledger.sqlite3"
)
bpr_narrow_owner_private(
    "${bpr_unavailable_root}/state"
    "${bpr_unavailable_root}/state/v1"
    "${bpr_unavailable_root}/state/v1/review-ledger.sqlite3"
)
bpr_run_review_ledger_probe(
    unavailable
    single
    "${bpr_unavailable_root}"
    111111011111110
    "REVIEW_LEDGER_COMPOSITION_EVIDENCE:"
)
if(NOT IS_DIRECTORY
    "${bpr_unavailable_root}/state/v1/review-ledger.sqlite3"
)
    message(FATAL_ERROR "the forced review-ledger obstruction was replaced")
endif()

file(REMOVE_RECURSE "${BPR_ROOT}" "${bpr_unavailable_root}")
message("REVIEW_LEDGER_COMPOSITION_CONTRACT_OK")
