cmake_minimum_required(VERSION 4.4.2)

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS BPR_SOURCE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(READ "${BPR_SOURCE}" bpr_application_source)
string(FIND
    "${bpr_application_source}"
    "exercise_defect_composition("
    probe_begin
)
string(FIND
    "${bpr_application_source}"
    "defect_composition_evidence("
    probe_end
)
if(probe_begin EQUAL -1 OR probe_end EQUAL -1 OR probe_end LESS_EQUAL probe_begin)
    message(FATAL_ERROR "defect public probe boundary is missing")
endif()
math(EXPR probe_length "${probe_end} - ${probe_begin}")
string(SUBSTRING
    "${bpr_application_source}"
    ${probe_begin}
    ${probe_length}
    probe_source
)
foreach(forbidden IN ITEMS
    "ReviewLedgerHost->persist("
    "ReviewMutationCoordinator->publish("
    "classify_defect("
    "dispatch_defect("
)
    string(FIND "${probe_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "defect fixture helper bypasses the public path: ${forbidden}")
    endif()
endforeach()
foreach(required_wire IN ITEMS
    "setCurrentIndex("
    "classify_defect_action()->trigger()"
    "left_selector()->setCurrentIndex("
    "right_selector()->setCurrentIndex("
    "verify_action()->trigger()"
    "reclassify_action()->trigger()"
    "publish_action()->trigger()"
)
    string(FIND "${probe_source}" "${required_wire}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "defect public trace omitted ${required_wire}")
    endif()
endforeach()
foreach(required_product IN ITEMS
    "application::correlate("
    "ReviewLedgerHost->persist("
    "classify_defect("
    "dispatch_defect("
)
    string(FIND "${bpr_application_source}" "${required_product}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "defect product composition omitted ${required_product}")
    endif()
endforeach()

math(EXPR bpr_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

function(bpr_run_defect_probe fixture fixture_label root course epoch_phase expected_evidence)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -E
            env
            "QT_QPA_PLATFORM=offscreen"
            "${BPR_APPLICATION}"
            --github-fixture
            "${fixture}"
            --epoch-fixture-root
            "${root}"
            --epoch-fixture-phase
            "${epoch_phase}"
            --defect-composition-probe
            "${course}"
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
        TIMEOUT ${bpr_timeout}
    )
    if(NOT probe_result EQUAL 0)
        message(FATAL_ERROR
            "defect ${course} process failed (${probe_result})\n"
            "stdout:\n${probe_output}\n"
            "stderr:\n${probe_error}"
        )
    endif()
    string(TOUPPER "${course}" course_marker)
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        GITHUB_FIXTURE_OK:${fixture_label}
        DEFECT_COMPOSITION_${course_marker}_OK
        DEFECT_COMPOSITION_EVIDENCE:${expected_evidence}
        WORKBENCH_HOST_COMPOSED_OK
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "defect ${course} process omitted ${marker}\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
endfunction()

file(REMOVE_RECURSE "${BPR_ROOT}")
foreach(fixture_spec IN ITEMS "github-com|github.com" "ghes-3.18|ghes-3.18")
    string(REPLACE "|" ";" fixture_parts "${fixture_spec}")
    list(GET fixture_parts 0 fixture)
    list(GET fixture_parts 1 fixture_label)
    set(fixture_root "${BPR_ROOT}/${fixture}")
    file(MAKE_DIRECTORY "${fixture_root}")
    file(CHMOD "${fixture_root}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
    bpr_run_defect_probe(
        "${fixture}"
        "${fixture_label}"
        "${fixture_root}"
        seed
        seed
        11111110011
    )
    bpr_run_defect_probe(
        "${fixture}"
        "${fixture_label}"
        "${fixture_root}"
        reopen
        reuse
        11111111110
    )
endforeach()
file(REMOVE_RECURSE "${BPR_ROOT}")
message("DEFECT_COMPOSITION_CONTRACT_OK")
