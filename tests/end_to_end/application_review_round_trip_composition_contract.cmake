cmake_minimum_required(VERSION 4.4.2)

foreach(required IN ITEMS
    BPR_APPLICATION
    BPR_APP_SOURCE
    BPR_ISOLATION_SCRIPT
    BPR_MANIFEST
    BPR_PYTHON
    BPR_ROOT
    BPR_RUN_BUDGET_MS
    BPR_SOURCE_ROOT
)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(READ "${BPR_APP_SOURCE}" bpr_application_source)
string(FIND
    "${bpr_application_source}"
    "exercise_review_round_trip_composition("
    probe_begin
)
string(FIND
    "${bpr_application_source}"
    "review_round_trip_composition_evidence("
    probe_end
)
if(probe_begin EQUAL -1 OR probe_end EQUAL -1 OR probe_end LESS_EQUAL probe_begin)
    message(FATAL_ERROR "review round-trip public probe boundary is missing")
endif()
math(EXPR probe_length "${probe_end} - ${probe_begin}")
string(SUBSTRING
    "${bpr_application_source}"
    ${probe_begin}
    ${probe_length}
    probe_source
)
foreach(forbidden IN ITEMS
    "ReviewMutationCoordinator->publish("
    "review_mutation_provider().submit("
    "ReviewProjection.push_back("
)
    string(FIND "${probe_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "review round-trip fixture helper bypasses the public path: ${forbidden}"
        )
    endif()
endforeach()
foreach(required_wire IN ITEMS
    "body_editor()->setPlainText("
    "publish_action()->trigger()"
    "report_review_mutation_target("
    "review_mutation_evidence()"
)
    string(FIND "${probe_source}" "${required_wire}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "review round-trip public trace omitted ${required_wire}"
        )
    endif()
endforeach()
foreach(required_product IN ITEMS
    "dispatch_review_draft("
    "ReviewMutationCoordinator->publish("
    "refresh_review_projection()"
)
    string(FIND "${bpr_application_source}" "${required_product}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "review round-trip product composition omitted ${required_product}"
        )
    endif()
endforeach()

file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${BPR_ROOT}")
math(EXPR bpr_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")
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
        --review-round-trip-composition-probe
        --automated-run-budget-ms
        "${BPR_RUN_BUDGET_MS}"
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_output
    ERROR_VARIABLE probe_error
    TIMEOUT ${bpr_timeout}
)
if(NOT probe_result EQUAL 0)
    message(FATAL_ERROR
        "review round-trip composition process failed (${probe_result})\n"
        "stdout:\n${probe_output}\n"
        "stderr:\n${probe_error}"
    )
endif()
foreach(marker IN ITEMS
    PUBLIC_PATH_OK
    GITHUB_FIXTURE_OK:github.com
    REVIEW_ROUND_TRIP_COMPOSITION_OK
    REVIEW_ROUND_TRIP_COMPOSITION_EVIDENCE:111111111
    WORKBENCH_HOST_COMPOSED_OK
)
    string(FIND "${probe_output}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "review round-trip composition process omitted ${marker}\n"
            "stdout:\n${probe_output}"
        )
    endif()
endforeach()

execute_process(
    COMMAND
        "${BPR_PYTHON}"
        "${BPR_ISOLATION_SCRIPT}"
        --root
        "${BPR_SOURCE_ROOT}"
        --manifest
        "${BPR_MANIFEST}"
    RESULT_VARIABLE isolation_result
    OUTPUT_VARIABLE isolation_output
    ERROR_VARIABLE isolation_error
)
if(NOT isolation_result EQUAL 0
    OR NOT isolation_output MATCHES "LIVE_MUTATION_ISOLATION_OK"
)
    message(FATAL_ERROR
        "live mutation isolation failed (${isolation_result})\n"
        "stdout:\n${isolation_output}\n"
        "stderr:\n${isolation_error}"
    )
endif()

file(REMOVE_RECURSE "${BPR_ROOT}")
message("REVIEW_ROUND_TRIP_COMPOSITION_CONTRACT_OK")
