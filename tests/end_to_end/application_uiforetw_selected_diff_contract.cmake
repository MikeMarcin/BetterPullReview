cmake_minimum_required(VERSION 4.4.2)

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS BPR_SOURCE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

# The attended capture must traverse the shipped drawer selection wire. Keep
# the test-only direct-diff helper compiled so the runtime counter can prove it
# was available but unused.
file(READ "${BPR_SOURCE}" bpr_application_source)
string(FIND
    "${bpr_application_source}"
    "auto detail::github_application_fixture_access::exercise_uiforetw_selected_diff("
    bpr_trace_start
)
string(FIND
    "${bpr_application_source}"
    "auto detail::github_application_fixture_access::uiforetw_selected_diff_evidence("
    bpr_trace_end
)
if(bpr_trace_start EQUAL -1
        OR bpr_trace_end EQUAL -1
        OR bpr_trace_end LESS_EQUAL bpr_trace_start)
    message(FATAL_ERROR "cannot isolate the UIforETW selected-diff trace")
endif()
math(EXPR bpr_trace_length "${bpr_trace_end} - ${bpr_trace_start}")
string(SUBSTRING
    "${bpr_application_source}"
    ${bpr_trace_start}
    ${bpr_trace_length}
    bpr_trace_source
)
string(FIND "${bpr_trace_source}" "request_fixture_diff" bpr_shortcut)
if(NOT bpr_shortcut EQUAL -1)
    message(FATAL_ERROR "UIforETW trace calls the fixture diff shortcut")
endif()
string(FIND
    "${bpr_application_source}"
    "auto detail::github_application_fixture_access::request_fixture_diff("
    bpr_helper
)
if(bpr_helper EQUAL -1)
    message(FATAL_ERROR "fixture diff helper is absent, making the isolation claim vacuous")
endif()

math(EXPR bpr_phase_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")
set(bpr_evidence_root "${BPR_ROOT}/evidence")
file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${bpr_evidence_root}")

foreach(profile IN ITEMS light dark high-contrast-light high-contrast-dark)
    set(profile_root "${BPR_ROOT}/${profile}")
    set(profile_evidence "${bpr_evidence_root}/${profile}")
    file(MAKE_DIRECTORY "${profile_root}")
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -E
            env
            "QT_QPA_PLATFORM=offscreen"
            "${BPR_APPLICATION}"
            --github-fixture
            uiforetw-74
            --epoch-fixture-root
            "${profile_root}"
            --epoch-fixture-phase
            single
            --uiforetw-selected-diff-evidence
            "${profile_evidence}"
            --diff-evidence-appearance
            "${profile}"
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
        TIMEOUT ${bpr_phase_timeout}
    )
    if(NOT probe_result EQUAL 0)
        message(FATAL_ERROR
            "UIforETW ${profile} capture failed (${probe_result})\n"
            "stdout:\n${probe_output}\nstderr:\n${probe_error}"
        )
    endif()
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        GITHUB_FIXTURE_OK:uiforetw-74
        UIFORETW_EXACT_COMMIT_PAIR_OK
        UIFORETW_RESOURCE_GESTURE_OK
        UIFORETW_RESOURCE_BLOBS_OK
        UIFORETW_RESOURCE_INTRALINE_OK
        UIFORETW_RESOURCE_ROLE_OK
        UIFORETW_RC_GESTURE_OK
        UIFORETW_RC_BLOBS_OK
        UIFORETW_RC_INTRALINE_OK
        UIFORETW_RC_ROLE_OK
        UIFORETW_FIXTURE_DIFF_HELPER_UNUSED_OK
        UIFORETW_SELECTED_DIFF_TRACE_OK
        UIFORETW_SELECTED_DIFF_EVIDENCE_OK
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "UIforETW ${profile} capture omitted ${marker}\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
    foreach(artifact IN ITEMS full-window.png intraline-comparison.png capture-facts.json)
        if(NOT EXISTS "${profile_evidence}/${artifact}")
            message(FATAL_ERROR "UIforETW ${profile} omitted ${artifact}")
        endif()
    endforeach()
    file(READ "${profile_evidence}/capture-facts.json" capture_facts)
    foreach(fact IN ITEMS
        "\"appearance\": \"${profile}\""
        "\"base_blob\": \"b995e189fb58da146945e1fb22380cdf4a7f4e47\""
        "\"head_blob\": \"6335c1b2e14cd5e63142ce1dfc283fcf538fb134\""
        "\"fixture_diff_helper_invocations\": 0"
        "\"selected_path\": \"UIforETW/UIforETW.rc\""
    )
        string(FIND "${capture_facts}" "${fact}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "UIforETW ${profile} capture facts omitted ${fact}\n${capture_facts}"
            )
        endif()
    endforeach()
endforeach()

set(cut_root "${BPR_ROOT}/cut-selection-wire")
execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -E
        env
        "QT_QPA_PLATFORM=offscreen"
        "${BPR_APPLICATION}"
        --github-fixture
        uiforetw-74
        --epoch-fixture-root
        "${cut_root}"
        --epoch-fixture-phase
        single
        --uiforetw-selected-diff-evidence
        "${cut_root}/evidence"
        --diff-evidence-appearance
        light
        --uiforetw-selected-diff-cut-selection-wire
        --automated-run-budget-ms
        "${BPR_RUN_BUDGET_MS}"
    RESULT_VARIABLE cut_result
    OUTPUT_VARIABLE cut_output
    ERROR_VARIABLE cut_error
    TIMEOUT ${bpr_phase_timeout}
)
if(cut_result EQUAL 0)
    message(FATAL_ERROR
        "UIforETW selected-diff trace passed with the real Qt wire cut\n"
        "stdout:\n${cut_output}\nstderr:\n${cut_error}"
    )
endif()
foreach(marker IN ITEMS
    UIFORETW_RESOURCE_GESTURE_OK
    UIFORETW_FIXTURE_DIFF_HELPER_UNUSED_OK
    UIFORETW_SELECTED_DIFF_TRACE_FAILED
)
    string(FIND "${cut_output}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "UIforETW cut-wire run omitted ${marker}\nstdout:\n${cut_output}"
        )
    endif()
endforeach()

message("UIFORETW_SELECTED_DIFF_CONTRACT_OK")
