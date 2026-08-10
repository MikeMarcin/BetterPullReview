cmake_minimum_required(VERSION 4.4.2)

# Forces only the workbench-layout database open to fail, then drives the
# fixture-backed shipped window through every composition wire BPR-0226 owns.
foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

math(EXPR bpr_phase_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY
    "${BPR_ROOT}/state/v1/workbench-settings.sqlite3"
)
file(
    CHMOD
    "${BPR_ROOT}"
    "${BPR_ROOT}/state"
    "${BPR_ROOT}/state/v1"
    "${BPR_ROOT}/state/v1/workbench-settings.sqlite3"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
)

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
        "${BPR_ROOT}"
        --epoch-fixture-phase
        single
        --workbench-composition-probe
        --automated-run-budget-ms
        "${BPR_RUN_BUDGET_MS}"
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_output
    ERROR_VARIABLE probe_error
    TIMEOUT ${bpr_phase_timeout}
)
if(NOT probe_result EQUAL 0)
    message(FATAL_ERROR
        "workbench composition process failed (${probe_result})\n"
        "stdout:\n${probe_output}\n"
        "stderr:\n${probe_error}"
    )
endif()

foreach(marker IN ITEMS
    PUBLIC_PATH_OK
    GITHUB_FIXTURE_OK:github.com
    REVIEW_LEDGER_OK
    APPEARANCE_PERSISTENCE_AVAILABLE
    WORKBENCH_AUTHORITY_WIRE_OK
    WORKBENCH_BOUNDED_OBSERVATION_OK
    WORKBENCH_INTENT_WIRE_OK
    WORKBENCH_PROJECTION_WIRE_OK
    WORKBENCH_CHANGE_WIRE_OK
    WORKBENCH_DRAWER_WIDTH_WIRE_OK
    WORKBENCH_SESSION_FALLBACK_OK
    WORKBENCH_IDENTITY_MISMATCH_OK
    WORKBENCH_INVALIDATION_WIRE_OK
    WORKBENCH_FAILURE_WIRE_OK
    WORKBENCH_COMPOSITION_TRACE_OK
    WORKBENCH_HOST_COMPOSED_OK
)
    string(FIND "${probe_output}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "workbench composition omitted ${marker}\nstdout:\n${probe_output}"
        )
    endif()
endforeach()

string(FIND "${probe_output}" "WORKBENCH_COMPOSITION_TRACE_FAILED" false_trace)
if(NOT false_trace EQUAL -1)
    message(FATAL_ERROR
        "workbench composition falsely reported failure\nstdout:\n${probe_output}"
    )
endif()
if(NOT IS_DIRECTORY "${BPR_ROOT}/state/v1/workbench-settings.sqlite3")
    message(FATAL_ERROR
        "the forced workbench-store obstruction was replaced by a database"
    )
endif()

file(REMOVE_RECURSE "${BPR_ROOT}")
message("WORKBENCH_COMPOSITION_CONTRACT_OK")
