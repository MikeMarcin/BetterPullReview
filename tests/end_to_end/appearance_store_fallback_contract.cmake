cmake_minimum_required(VERSION 4.4.2)

# Proves the real application keeps appearance and syntax composed when only
# the appearance database cannot open. The obstruction is the exact database
# path, so the epoch catalog, review ledger, and workbench settings continue to
# use the same valid owner-private state directory.

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

math(EXPR bpr_phase_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

function(require_literal label output marker)
    string(FIND "${output}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "${label} omitted ${marker}\nstdout:\n${output}")
    endif()
endfunction()

function(reject_literal label output marker)
    string(FIND "${output}" "${marker}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "${label} falsely reported ${marker}\nstdout:\n${output}")
    endif()
endfunction()

function(require_regex label output pattern)
    string(REGEX MATCH "${pattern}" matched "${output}")
    if(matched STREQUAL "")
        message(FATAL_ERROR
            "${label} did not match ${pattern}\nstdout:\n${output}"
        )
    endif()
endfunction()

function(run_phase label output_name)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -E
            env
            "QT_QPA_PLATFORM=offscreen"
            "${BPR_APPLICATION}"
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
            ${ARGN}
        RESULT_VARIABLE phase_result
        OUTPUT_VARIABLE phase_output
        ERROR_VARIABLE phase_error
        TIMEOUT ${bpr_phase_timeout}
    )
    if(NOT phase_result EQUAL 0)
        # [[BPR-0555]]. The child dumps its whole marker block below, and
        # `RUN_BUDGET_EXPIRED` is in it, but a reader sees the headline. A run
        # that merely ran out of time on a busy host said "label process
        # failed" and read as a defect in the phase; say which it was.
        set(bpr_budget_headline "")
        if(phase_output MATCHES "RUN_BUDGET_EXPIRED:[^\n]*")
            set(bpr_budget_headline "${CMAKE_MATCH_0}\n")
        endif()
        message(FATAL_ERROR
            "${label} process failed (${phase_result})\n"
            "${bpr_budget_headline}"
            "stdout:\n${phase_output}\n"
            "stderr:\n${phase_error}"
        )
    endif()
    set(${output_name} "${phase_output}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY
    "${BPR_ROOT}/state/v1/appearance-settings.sqlite3"
)
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")
bpr_narrow_owner_private(
    "${BPR_ROOT}"
    "${BPR_ROOT}/state"
    "${BPR_ROOT}/state/v1"
    "${BPR_ROOT}/state/v1/appearance-settings.sqlite3"
)

# Process one drives the session-only settings surface and a selected diff.
# Separate checks distinguish host construction, profile-to-window adoption,
# classifier construction, and classified spans reaching the panes.
run_phase(
    "fallback session"
    fallback_output
    --github-fixture
    github-com
    --epoch-fixture-root
    "${BPR_ROOT}"
    --epoch-fixture-phase
    single
    --appearance-gesture-probe
    --diff-workbench-probe
    --diff-interaction-evidence
    "${BPR_ROOT}/diff-evidence"
    --render-evidence
    "${BPR_ROOT}/fallback-window.png"
)
foreach(marker IN ITEMS
    "PUBLIC_PATH_OK"
    "REVIEW_LEDGER_OK"
    "WORKBENCH_HOST_COMPOSED_OK"
    "APPEARANCE_HOST_COMPOSED_OK"
    "SYNTAX_CLASSIFIER_COMPOSED_OK"
    "SYNTAX_THEME_REPAINT_OK"
    "APPEARANCE_GESTURE_OK"
    "APPEARANCE_APPLIED:"
    ":from-profile"
    "APPEARANCE_PERSISTENCE_UNAVAILABLE"
    "APPEARANCE_DURABLE_WRITES:0"
)
    require_literal("fallback session" "${fallback_output}" "${marker}")
endforeach()
require_regex(
    "fallback session"
    "${fallback_output}"
    "SYNTAX_CLASSIFICATION:[1-9][0-9]*:[1-9][0-9]*"
)
reject_literal(
    "fallback session"
    "${fallback_output}"
    "APPEARANCE_PERSISTENCE_AVAILABLE"
)

# Process two reuses the exact application-data root without a gesture. A
# session-only port must have no record to restore, so System and tab width four
# return even though process one selected another mode and width.
run_phase(
    "fallback restart"
    restart_output
    --self-test
    --test-application-data-root
    "${BPR_ROOT}"
)
foreach(marker IN ITEMS
    "PUBLIC_PATH_OK"
    "APPEARANCE_HOST_COMPOSED_OK"
    "APPEARANCE_SETTINGS:system:4"
    "APPEARANCE_PERSISTENCE_UNAVAILABLE"
    "APPEARANCE_DURABLE_WRITES:0"
)
    require_literal("fallback restart" "${restart_output}" "${marker}")
endforeach()
reject_literal(
    "fallback restart"
    "${restart_output}"
    "APPEARANCE_PERSISTENCE_AVAILABLE"
)

file(REMOVE_RECURSE "${BPR_ROOT}")
message("APPEARANCE_STORE_FALLBACK_CONTRACT_OK")
