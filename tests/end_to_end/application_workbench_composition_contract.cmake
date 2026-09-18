cmake_minimum_required(VERSION 4.4.2)

# Forces only the workbench-layout database open to fail, then drives the
# fixture-backed shipped window through BPR-0226 composition and BPR-0227's
# selected-file diff wire.
foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS BPR_SOURCE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(READ "${BPR_SOURCE}" bpr_application_source)
string(FIND
    "${bpr_application_source}"
    "auto detail::github_application_fixture_access::exercise_workbench_composition("
    bpr_workbench_trace_start
)
string(FIND
    "${bpr_application_source}"
    "auto detail::github_application_fixture_access::workbench_composition_evidence("
    bpr_workbench_trace_end
)
if(bpr_workbench_trace_start EQUAL -1
        OR bpr_workbench_trace_end EQUAL -1
        OR bpr_workbench_trace_end LESS_EQUAL bpr_workbench_trace_start)
    message(FATAL_ERROR
        "cannot isolate the nonempty workbench selected-diff trace implementation"
    )
endif()
math(EXPR
    bpr_workbench_trace_length
    "${bpr_workbench_trace_end} - ${bpr_workbench_trace_start}"
)
string(SUBSTRING
    "${bpr_application_source}"
    ${bpr_workbench_trace_start}
    ${bpr_workbench_trace_length}
    bpr_workbench_trace_source
)
string(FIND "${bpr_workbench_trace_source}" "request_fixture_diff" fixture_diff_shortcut)
if(NOT fixture_diff_shortcut EQUAL -1)
    message(FATAL_ERROR
        "workbench selected-diff trace still calls the fixture diff shortcut"
    )
endif()
string(FIND
    "${bpr_application_source}"
    "auto detail::github_application_fixture_access::request_fixture_diff("
    fixture_diff_helper_implementation
)
if(fixture_diff_helper_implementation EQUAL -1)
    message(FATAL_ERROR
        "fixture diff helper must remain present for shortcut-isolation evidence"
    )
endif()

math(EXPR bpr_phase_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")

function(bpr_prepare_workbench_root root)
    file(REMOVE_RECURSE "${root}")
    file(MAKE_DIRECTORY "${root}/state/v1/workbench-settings.sqlite3")
    bpr_narrow_owner_private(
        "${root}"
        "${root}/state"
        "${root}/state/v1"
        "${root}/state/v1/workbench-settings.sqlite3"
    )
endfunction()

bpr_private_data_root(BPR_ROOT)
bpr_prepare_workbench_root("${BPR_ROOT}")

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
    APPEARANCE_PERSISTENCE_AVAILABLE
    WORKBENCH_AUTHORITY_WIRE_OK
    WORKBENCH_BOUNDED_OBSERVATION_OK
    WORKBENCH_INTENT_WIRE_OK
    WORKBENCH_PROJECTION_WIRE_OK
    WORKBENCH_FILE_GESTURE_OK
    WORKBENCH_SELECTED_DIFF_REQUEST_OK
    WORKBENCH_EXACT_COMPARISON_OK
    WORKBENCH_CHANGE_WIRE_OK
    WORKBENCH_DRAWER_WIDTH_WIRE_OK
    WORKBENCH_SESSION_FALLBACK_OK
    WORKBENCH_IDENTITY_MISMATCH_OK
    WORKBENCH_INVALIDATION_WIRE_OK
    WORKBENCH_FAILURE_WIRE_OK
    WORKBENCH_FIXTURE_DIFF_HELPER_UNUSED_OK
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

set(bpr_cut_wire_root "${BPR_ROOT}-cut-selection-wire")
bpr_prepare_workbench_root("${bpr_cut_wire_root}")
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
        "${bpr_cut_wire_root}"
        --epoch-fixture-phase
        single
        --workbench-composition-probe
        --workbench-composition-cut-selection-wire
        --automated-run-budget-ms
        "${BPR_RUN_BUDGET_MS}"
    RESULT_VARIABLE cut_wire_result
    OUTPUT_VARIABLE cut_wire_output
    ERROR_VARIABLE cut_wire_error
    TIMEOUT ${bpr_phase_timeout}
)
if(cut_wire_result EQUAL 0)
    message(FATAL_ERROR
        "workbench composition passed with the real Qt selection wire cut\n"
        "stdout:\n${cut_wire_output}\n"
        "stderr:\n${cut_wire_error}"
    )
endif()
foreach(marker IN ITEMS
    WORKBENCH_FILE_GESTURE_OK
    WORKBENCH_INTENT_WIRE_FAILED
    WORKBENCH_FIXTURE_DIFF_HELPER_UNUSED_OK
    WORKBENCH_COMPOSITION_TRACE_FAILED
)
    string(FIND "${cut_wire_output}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "cut selection wire run omitted ${marker}\nstdout:\n${cut_wire_output}"
        )
    endif()
endforeach()

file(REMOVE_RECURSE "${BPR_ROOT}")
file(REMOVE_RECURSE "${bpr_cut_wire_root}")
message("WORKBENCH_COMPOSITION_CONTRACT_OK")
