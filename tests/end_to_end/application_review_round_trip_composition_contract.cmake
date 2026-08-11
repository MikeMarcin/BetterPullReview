cmake_minimum_required(VERSION 4.4.2)

macro(bpr_require_dormant_provider_role pattern expected role)
    string(REGEX MATCHALL
        "${pattern}"
        bpr_dormant_provider_role_matches
        "${bpr_dormant_provider_source}"
    )
    list(LENGTH
        bpr_dormant_provider_role_matches
        bpr_dormant_provider_role_count
    )
    if(NOT bpr_dormant_provider_role_count EQUAL ${expected})
        message(FATAL_ERROR
            "dormant concrete-provider inventory expected ${expected} "
            "${role} occurrence(s), found "
            "${bpr_dormant_provider_role_count}"
        )
    endif()
endmacro()

function(bpr_verify_dormant_provider_inventory source_variable)
    set(bpr_dormant_provider_source "${${source_variable}}")

    bpr_require_dormant_provider_role(
        "#include[ \t]+\"live_review_mutation_provider\\.hpp\""
        1
        "provider include"
    )
    bpr_require_dormant_provider_role(
        "using[ \t]+adapters::github::live_review_mutation_provider"
        1
        "provider-type using declaration"
    )
    bpr_require_dormant_provider_role(
        "using[ \t]+adapters::github::make_live_review_mutation_provider"
        1
        "provider-factory using declaration"
    )
    bpr_require_dormant_provider_role(
        "LiveReviewMutationProvider\\.reset\\(\\)"
        2
        "production reset"
    )
    bpr_require_dormant_provider_role(
        "auto[ \t]+provider[ \t]*=[ \t]*make_live_review_mutation_provider\\("
        1
        "provider factory call"
    )
    bpr_require_dormant_provider_role(
        "LiveReviewMutationProvider[ \t]*=[ \t]*std::move\\(\\*provider\\)"
        1
        "provider assignment"
    )
    bpr_require_dormant_provider_role(
        "std::unique_ptr<live_review_mutation_provider>[ \t]+LiveReviewMutationProvider"
        1
        "production-only provider member"
    )
    string(REGEX MATCH
        "LiveReviewMutationProvider[ \t\r\n]*(->|\\.)[ \t\r\n]*(observe_target|submit|publish)[ \t\r\n]*\\("
        bpr_forbidden_dormant_provider_use
        "${bpr_dormant_provider_source}"
    )
    if(NOT bpr_forbidden_dormant_provider_use STREQUAL "")
        message(FATAL_ERROR
            "dormant concrete provider performs an observe, submit, or "
            "publish action outside its allowlisted lifecycle roles"
        )
    endif()

    bpr_require_dormant_provider_role(
        "live_review_mutation_provider"
        5
        "lower-case concrete-provider token"
    )
    bpr_require_dormant_provider_role(
        "make_live_review_mutation_provider"
        2
        "provider-factory token"
    )
    bpr_require_dormant_provider_role(
        "LiveReviewMutationProvider"
        4
        "provider-member token"
    )

    string(REGEX MATCH
        "#if[ \t]+defined\\(BPR_BUILD_TESTING\\)[ \t\r\n]+std::unique_ptr<application::review_mutation_coordinator>[ \t]+ReviewMutationCoordinator[ \t\r\n]*;[ \t\r\n]+#else[ \t\r\n]+std::unique_ptr<live_review_mutation_provider>[ \t]+LiveReviewMutationProvider[ \t\r\n]*;[ \t\r\n]+#endif"
        bpr_dormant_provider_member_branch
        "${bpr_dormant_provider_source}"
    )
    if(bpr_dormant_provider_member_branch STREQUAL "")
        message(FATAL_ERROR
            "dormant concrete-provider inventory omitted its "
            "production-only provider member branch"
        )
    endif()

    string(REGEX MATCH
        "#if[ \t]+defined\\(BPR_BUILD_TESTING\\)[ \t\r\n]+ReviewMutationCoordinator\\.reset\\(\\);[ \t\r\n]+#else[ \t\r\n]+LiveReviewMutationProvider\\.reset\\(\\);[ \t\r\n]+#endif"
        bpr_dormant_provider_lifecycle_reset_branch
        "${bpr_dormant_provider_source}"
    )
    if(bpr_dormant_provider_lifecycle_reset_branch STREQUAL "")
        message(FATAL_ERROR
            "dormant concrete-provider inventory omitted its "
            "production-only lifecycle reset branch"
        )
    endif()
endfunction()

function(bpr_verify_dormant_production_branch_digest source_variable)
    set(bpr_dormant_branch_source "${${source_variable}}")
    string(FIND
        "${bpr_dormant_branch_source}"
        "void compose_review_mutation("
        bpr_dormant_compose_begin
    )
    string(FIND
        "${bpr_dormant_branch_source}"
        "void refresh_review_projection()"
        bpr_dormant_compose_end
    )
    if(bpr_dormant_compose_begin EQUAL -1
        OR bpr_dormant_compose_end EQUAL -1
        OR bpr_dormant_compose_end LESS_EQUAL bpr_dormant_compose_begin
    )
        message(FATAL_ERROR
            "review mutation composition boundary is missing for digest lock"
        )
    endif()
    math(EXPR bpr_dormant_compose_length
        "${bpr_dormant_compose_end} - ${bpr_dormant_compose_begin}"
    )
    string(SUBSTRING
        "${bpr_dormant_branch_source}"
        ${bpr_dormant_compose_begin}
        ${bpr_dormant_compose_length}
        bpr_dormant_compose_source
    )
    string(FIND
        "${bpr_dormant_compose_source}"
        "#else"
        bpr_dormant_production_marker
    )
    if(bpr_dormant_production_marker EQUAL -1)
        message(FATAL_ERROR
            "tests-disabled production review mutation branch is missing"
        )
    endif()
    string(LENGTH "#else" bpr_dormant_production_marker_length)
    math(EXPR bpr_dormant_production_begin
        "${bpr_dormant_production_marker} + ${bpr_dormant_production_marker_length}"
    )
    string(FIND
        "${bpr_dormant_compose_source}"
        "#endif"
        bpr_dormant_production_end
    )
    if(bpr_dormant_production_end LESS_EQUAL bpr_dormant_production_begin)
        message(FATAL_ERROR
            "tests-disabled production review mutation branch is malformed"
        )
    endif()
    math(EXPR bpr_dormant_production_length
        "${bpr_dormant_production_end} - ${bpr_dormant_production_begin}"
    )
    string(SUBSTRING
        "${bpr_dormant_compose_source}"
        ${bpr_dormant_production_begin}
        ${bpr_dormant_production_length}
        bpr_dormant_production_source
    )
    string(REPLACE
        "\r\n"
        "\n"
        bpr_dormant_production_source
        "${bpr_dormant_production_source}"
    )
    string(REPLACE
        "\r"
        "\n"
        bpr_dormant_production_source
        "${bpr_dormant_production_source}"
    )
    string(SHA256
        bpr_dormant_production_digest
        "${bpr_dormant_production_source}"
    )
    # SHA-256 of the exact LF-normalized bytes after the branch's #else and
    # before its #endif. Tokens and horizontal whitespace remain unmodified.
    set(bpr_expected_dormant_production_digest
        "cb4661decfd4d910861a1d1f4f237dd085a14a6bada3b12ac30c7c03046b7243"
    )
    if(NOT bpr_dormant_production_digest STREQUAL
        bpr_expected_dormant_production_digest
    )
        message(FATAL_ERROR
            "tests-disabled production compose_review_mutation branch "
            "digest drifted: expected "
            "${bpr_expected_dormant_production_digest}, found "
            "${bpr_dormant_production_digest}"
        )
    endif()
endfunction()

if(DEFINED BPR_PROVIDER_INVENTORY_SOURCE_FILE)
    if(NOT EXISTS "${BPR_PROVIDER_INVENTORY_SOURCE_FILE}")
        message(FATAL_ERROR
            "provider inventory source does not exist: "
            "${BPR_PROVIDER_INVENTORY_SOURCE_FILE}"
        )
    endif()
    file(READ
        "${BPR_PROVIDER_INVENTORY_SOURCE_FILE}"
        bpr_provider_inventory_source
    )
    bpr_verify_dormant_provider_inventory(bpr_provider_inventory_source)
    bpr_verify_dormant_production_branch_digest(
        bpr_provider_inventory_source
    )
    message("DORMANT_PROVIDER_INVENTORY_OK")
    return()
endif()

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
bpr_verify_dormant_provider_inventory(bpr_application_source)
bpr_verify_dormant_production_branch_digest(bpr_application_source)
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

string(FIND
    "${bpr_application_source}"
    "void compose_review_mutation("
    mutation_composition_begin
)
string(FIND
    "${bpr_application_source}"
    "void refresh_review_projection()"
    mutation_composition_end
)
if(mutation_composition_begin EQUAL -1
    OR mutation_composition_end EQUAL -1
    OR mutation_composition_end LESS_EQUAL mutation_composition_begin
)
    message(FATAL_ERROR "review mutation composition boundary is missing")
endif()
math(EXPR mutation_composition_length
    "${mutation_composition_end} - ${mutation_composition_begin}"
)
string(SUBSTRING
    "${bpr_application_source}"
    ${mutation_composition_begin}
    ${mutation_composition_length}
    mutation_composition_source
)
string(FIND "${mutation_composition_source}" "#else" production_begin)
string(FIND "${mutation_composition_source}" "#endif" production_end)
if(production_begin EQUAL -1
    OR production_end EQUAL -1
    OR production_end LESS_EQUAL production_begin
)
    message(FATAL_ERROR "production review mutation branch is missing")
endif()
math(EXPR production_length "${production_end} - ${production_begin}")
string(SUBSTRING
    "${mutation_composition_source}"
    ${production_begin}
    ${production_length}
    production_mutation_source
)
foreach(required_dormant IN ITEMS
    "select_designated_review_mutation("
    "accepted_review_mutation_transport()"
    "LiveReviewMutationProvider.reset()"
    "make_live_review_mutation_provider("
    "LiveReviewMutationProvider ="
)
    string(FIND "${production_mutation_source}" "${required_dormant}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "dormant production review mutation composition omitted ${required_dormant}"
        )
    endif()
endforeach()
foreach(forbidden_live_action IN ITEMS
    "ReviewMutationCoordinator ="
    ".observe_target("
    "->observe_target("
    ".submit("
    "->submit("
    ".publish("
    "->publish("
)
    string(FIND
        "${production_mutation_source}"
        "${forbidden_live_action}"
        found
    )
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "dormant production composition performs ${forbidden_live_action}"
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
    REVIEW_MUTATION_PROVIDER:fixture
    REVIEW_ROUND_TRIP_COMPOSITION_OK
    REVIEW_ROUND_TRIP_COMPOSITION_EVIDENCE:111111111
    DORMANT_LIVE_PROVIDER_SELECTION:1:28/28:1:1
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
string(FIND "${probe_output}" "REVIEW_MUTATION_PROVIDER:live" live_marker)
if(NOT live_marker EQUAL -1)
    message(FATAL_ERROR
        "development fixture selected the live review mutation provider\n"
        "stdout:\n${probe_output}"
    )
endif()

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
