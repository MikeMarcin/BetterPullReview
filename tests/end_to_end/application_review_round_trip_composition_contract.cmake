cmake_minimum_required(VERSION 4.4.2)

function(bpr_require_literal_count source_variable token expected description)
    set(bpr_count_source "${${source_variable}}")
    string(LENGTH "${bpr_count_source}" bpr_count_before)
    string(LENGTH "${token}" bpr_count_token_length)
    if(bpr_count_token_length EQUAL 0)
        message(FATAL_ERROR "${description} uses an empty inventory token")
    endif()
    string(REPLACE "${token}" "" bpr_count_without "${bpr_count_source}")
    string(LENGTH "${bpr_count_without}" bpr_count_after)
    math(EXPR bpr_count
        "(${bpr_count_before} - ${bpr_count_after}) / ${bpr_count_token_length}"
    )
    if(NOT bpr_count EQUAL expected)
        message(FATAL_ERROR
            "${description} literal inventory expected ${expected}, found "
            "${bpr_count}: ${token}"
        )
    endif()
endfunction()

# Resealing the pinned digests ([[BPR-0383]], [[BPR-0819]])
#
# The three digests below are review tripwires, not build products. A pin that
# goes red asks for a review of what changed inside its scope; it does not ask
# for a new number. Reseal only in the commit that records that review, on the
# card that owns the change, listing every commit the new values cover:
#
#   1. List the changes since the pins last matched:
#        git log <last-matching-commit>..HEAD -- src/app/src/github_application.cpp \
#            <each file in bpr_production_app_sources>
#   2. Confirm each change inside a pinned scope has a recorded review, and
#      record one for any that does not.
#   3. Print the current values, which verifies nothing:
#        cmake -DBPR_PRINT_PINNED_DIGESTS=ON -DBPR_SOURCE_ROOT=<checkout> \
#            -P tests/end_to_end/application_review_round_trip_composition_contract.cmake
#   4. Paste the printed values into the pins, and name the reviewed range in the
#      commit.
#
# Every value is the SHA-256 of LF-normalized bytes. The functions below compute
# them, and both the checks and step 3 use those same functions.
# BPR-0242 reseal: d6dfca2b..2a64e901 is reviewed in
# docs/rfcs/plans/rfc-bpr-0008-owner-closeout/composition-reseal-review.md.
# BPR-0933 reseal: 12d032cf changes only the read opt-in's presentation in
# github_application.cpp; the review is recorded on BPR-0933.
# BPR-0982 reseal: the main.cpp self-test CLI course follows the discovered
# CLI entry (BPR-0909); no product path changes. Review recorded on BPR-0982.
# Merge reseal (codex/rfc-0018-0026-closeout into main): the branch's
# 8d072d14 and 9210330a are reviewed on BPR-0976 and BPR-0977; 6f8e44e4
# changes only the BPR_BUILD_TESTING fixture desktop factory, reviewed on
# BPR-0978; main's 12d032cf and 841999ad are covered above. No other change.
# BPR-0984 reseal: the BPR_BUILD_TESTING workbench composition probe ignores
# the window's observed geometry in its session-fallback comparison; no
# product path changes. Review recorded on BPR-0984.
# BPR-0987 reseal: the setup open reads a pasted link's typed location, keeps
# the pull request the window already shows instead of reading it again, and
# places the location after acceptance; a Recently viewed row of the shown pull
# request lands on Overview without a read. No credential, provider, vault or
# mutation path changes. Review recorded on BPR-0987.
# BPR-0987 correction: the pins above were taken before BPR-0987's last edit
# (a null accepted-host guard in the setup open, a review location that is not
# read explained as not read, and a file location placed only once the epoch
# holds the shown pull request's authority), so they never matched 17e40dc8.
# Resealed to the committed file; no other change. Review recorded on BPR-0987.
function(bpr_lf_normalized_sha256 content out_variable)
    string(REPLACE "\r\n" "\n" bpr_normalized "${content}")
    string(REPLACE "\r" "\n" bpr_normalized "${bpr_normalized}")
    string(SHA256 bpr_normalized_digest "${bpr_normalized}")
    set(${out_variable} "${bpr_normalized_digest}" PARENT_SCOPE)
endfunction()

function(bpr_compute_full_application_digest source_variable out_variable)
    bpr_lf_normalized_sha256("${${source_variable}}" bpr_full_digest)
    set(${out_variable} "${bpr_full_digest}" PARENT_SCOPE)
endfunction()

function(bpr_verify_full_application_digest source_variable)
    bpr_compute_full_application_digest(
        ${source_variable}
        bpr_full_application_digest
    )
	set(bpr_expected_full_application_digest
		"07c49397d8225016655447cf2d48f7a997272087147ca961859bb995664e0955"
	)
    if(NOT bpr_full_application_digest STREQUAL
        bpr_expected_full_application_digest
    )
        message(FATAL_ERROR
            "full LF-normalized github_application.cpp digest drifted: "
            "expected ${bpr_expected_full_application_digest}, found "
            "${bpr_full_application_digest}"
        )
    endif()
endfunction()

function(bpr_compute_production_app_manifest_digest app_source_root out_variable)
    set(bpr_production_app_sources
        "include/bpr/app/foundation_application.hpp"
        "include/bpr/app/github_application.hpp"
        "include/bpr/app/native_filesystem_path.hpp"
        "include/bpr/app/profile_desktop.hpp"
        "include/bpr/app/release_maintenance_controls.hpp"
        "include/bpr/app/release_update_desktop.hpp"
        "include/bpr/app/worker_executor.hpp"
		"main.cpp"
		"src/designated_live_macos_supervisor.cpp"
		"src/designated_live_macos_supervisor_identity.cpp"
		"src/designated_live_macos_supervisor_identity.hpp"
		"src/designated_live_macos_supervisor_launch.cpp"
		"src/designated_live_macos_supervisor_launch.hpp"
        "src/foundation_application.cpp"
        "src/github_application.cpp"
        "src/installed_runtime.cpp"
        "src/installed_runtime.hpp"
        "src/platform_credential_vault.cpp"
        "src/platform_credential_vault.hpp"
        "src/profile_desktop.cpp"
        "src/release_maintenance_controls.cpp"
        "src/release_update_desktop.cpp"
        "src/review_submission_workflow.cpp"
        "src/review_submission_workflow.hpp"
		"src/windows_title_row_host.cpp"
		"src/windows_title_row_host.hpp"
		"src/worker_executor.cpp"
        "update_helper_main.cpp"
    )
    set(bpr_production_app_manifest "")
    foreach(relative_source IN LISTS bpr_production_app_sources)
        file(READ
            "${app_source_root}/${relative_source}"
            bpr_app_source_contents
        )
        bpr_lf_normalized_sha256("${bpr_app_source_contents}" bpr_app_source_digest)
        string(APPEND
            bpr_production_app_manifest
            "${relative_source}\n${bpr_app_source_digest}\n"
        )
    endforeach()
    string(SHA256
        bpr_manifest_digest
        "${bpr_production_app_manifest}"
    )
    set(${out_variable} "${bpr_manifest_digest}" PARENT_SCOPE)
endfunction()

function(bpr_verify_app_source_tree app_source_root)
    if(NOT IS_DIRECTORY "${app_source_root}")
        message(FATAL_ERROR
            "application source-tree inventory root does not exist: "
            "${app_source_root}"
        )
    endif()
    set(bpr_expected_app_sources
        "include/bpr/app/foundation_application.hpp"
        "include/bpr/app/github_application.hpp"
        "include/bpr/app/native_filesystem_path.hpp"
        "include/bpr/app/profile_desktop.hpp"
        "include/bpr/app/release_maintenance_controls.hpp"
        "include/bpr/app/release_update_desktop.hpp"
        "include/bpr/app/worker_executor.hpp"
        "main.cpp"
		"src/designated_live_macos_capsule_identity.cpp"
		"src/designated_live_macos_capsule_identity.hpp"
		"src/designated_live_macos_provenance.cpp"
		"src/designated_live_macos_provenance.hpp"
		"src/designated_live_macos_supervisor.cpp"
		"src/designated_live_macos_supervisor_identity.cpp"
		"src/designated_live_macos_supervisor_identity.hpp"
		"src/designated_live_macos_supervisor_launch.cpp"
		"src/designated_live_macos_supervisor_launch.hpp"
        "src/foundation_application.cpp"
        "src/github_application.cpp"
        "src/github_application_fixture_access.hpp"
        "src/github_public_path_fixture.cpp"
        "src/github_public_path_fixture.hpp"
        "src/installed_runtime.cpp"
        "src/installed_runtime.hpp"
        "src/large_review_fixture.hpp"
        "src/platform_credential_vault.cpp"
        "src/platform_credential_vault.hpp"
        "src/profile_desktop.cpp"
        "src/release_maintenance_controls.cpp"
        "src/release_update_desktop.cpp"
        "src/review_submission_fixture.cpp"
        "src/review_submission_fixture.hpp"
        "src/review_submission_workflow.cpp"
        "src/review_submission_workflow.hpp"
        "src/system_browser_opener.cpp"
        "src/system_browser_opener.hpp"
		"src/windows_title_row_host.cpp"
		"src/windows_title_row_host.hpp"
		"src/worker_executor.cpp"
        "update_helper_main.cpp"
    )
    file(GLOB_RECURSE bpr_actual_app_sources
        LIST_DIRECTORIES false
        RELATIVE "${app_source_root}"
        "${app_source_root}/*.c"
        "${app_source_root}/*.cc"
        "${app_source_root}/*.cpp"
        "${app_source_root}/*.cxx"
        "${app_source_root}/*.h"
        "${app_source_root}/*.hpp"
    )
    list(SORT bpr_actual_app_sources)
    if(NOT bpr_actual_app_sources STREQUAL bpr_expected_app_sources)
        message(FATAL_ERROR
            "application source-tree inventory drifted: expected "
            "${bpr_expected_app_sources}, found ${bpr_actual_app_sources}"
        )
    endif()

    bpr_compute_production_app_manifest_digest(
        "${app_source_root}"
        bpr_production_app_manifest_digest
    )
	set(bpr_expected_production_app_manifest_digest
        "9ad3bbe313fc77b9fae7aa87f9a9b9cd05cc5d29b1af111504359b646d55f615"
	)
    if(NOT bpr_production_app_manifest_digest STREQUAL
        bpr_expected_production_app_manifest_digest
    )
        message(FATAL_ERROR
            "production application source content digest drifted: expected "
            "${bpr_expected_production_app_manifest_digest}, found "
            "${bpr_production_app_manifest_digest}"
        )
    endif()
endfunction()

function(bpr_verify_replay_operator_surface app_source_root)
    file(READ "${app_source_root}/main.cpp" bpr_main_source)
    file(READ
        "${app_source_root}/include/bpr/app/github_application.hpp"
        bpr_app_header_source
    )
    foreach(required IN ITEMS
        "--initialize-or-reset-designated-live-replay-state"
        "argc != 2"
        "initialize_or_reset_designated_live_replay_state("
        "DESIGNATED_LIVE_REPLAY_STATE_READY"
        "config.ApplicationDataRoot"
    )
        string(FIND "${bpr_main_source}" "${required}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "replay operator/startup surface omitted ${required}"
            )
        endif()
    endforeach()
    foreach(required IN ITEMS
        "initialize_or_reset_designated_live_replay_state("
        "This is an operator operation, not an application-startup"
    )
        string(FIND "${bpr_app_header_source}" "${required}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "replay operator API omitted ${required}"
            )
        endif()
    endforeach()
    bpr_require_literal_count(
        bpr_main_source
        "--initialize-or-reset-designated-live-replay-state"
        1
        "no-path replay initializer argument"
    )
    bpr_require_literal_count(
        bpr_main_source
        "initialize_or_reset_designated_live_replay_state("
        1
        "replay initializer invocation"
    )
    foreach(forbidden IN ITEMS
        "designated-live-review-replay-state-v1"
        "NamespaceId"
        "QTextStream{ stdout } << applicationDataRoot"
    )
        string(FIND "${bpr_main_source}" "${forbidden}" found)
        if(NOT found EQUAL -1)
            message(FATAL_ERROR
                "replay path or identity leaked into the CLI surface: ${forbidden}"
            )
        endif()
    endforeach()
endfunction()

function(bpr_verify_async_mutation_host_inventory source_variable)
    set(bpr_async_mutation_source "${${source_variable}}")
    foreach(required IN ITEMS
        "#include \"live_review_mutation_provider.hpp\""
        "using adapters::github::make_live_review_mutation_provider;"
        "auto provider = make_live_review_mutation_provider("
		"adapters::sqlite::make_review_mutation_recovery_store("
		"std::move(*recovery)"
		"application::detail::designated_review_authority_bootstrap::run("
		"application::detail::designated_review_replay_initializer::load("
		".Replay             = std::move(*replay)"
        "adapters::github::detail::make_designated_review_public_observer("
        "ReviewMutationHost = std::make_unique<"
        "application::review_mutation_application_host>("
        "std::move(LiveReviewMutationGrant)"
        "worker_executor                                         ReviewMutationEffects;"
        "std::unique_ptr<application::review_mutation_application_host>"
    )
        string(FIND "${bpr_async_mutation_source}" "${required}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "asynchronous mutation-host inventory omitted ${required}"
            )
        endif()
    endforeach()
    bpr_require_literal_count(
        bpr_async_mutation_source
        "application::detail::review_mutation_user_authority_issuer::issue("
        1
        "normal exact user-publish issuer"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "make_live_review_mutation_provider("
        1
        "live review mutation provider factory call"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "ReviewMutationHost->publish("
        1
        "review mutation host publication call"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "ReviewMutationHost = std::make_unique<"
        2
        "fixture and production mutation host construction"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "std::unique_ptr<application::review_mutation_port>"
        1
        "production provider erasure"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "review_mutation_port"
        1
        "whole-app mutation port reference"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "make_review_mutation_testing_grant("
        1
        "fixture-only mutation grant factory call"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
		"application::detail::designated_review_authority_bootstrap::run("
        1
        "attended production authority bootstrap call"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
		"application::detail::designated_review_replay_initializer::load("
        1
        "attended replay-state load"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
		"application::detail::designated_review_replay_initializer::initialize_or_reset("
        1
        "explicit replay-state operator operation"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
		".Replay             = std::move(*replay)"
        1
        "explicit replay configuration handoff"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "adapters::github::detail::make_designated_review_public_observer("
        1
        "attended public observation adapter construction"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "std::move(LiveReviewMutationGrant)"
        1
        "production one-use execution grant handoff"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "application::review_mutation_execution_grant{ }"
        2
        "per-gesture authority starts absent"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "review_mutation_coordinator"
        0
        "whole-app direct mutation coordinator reference"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        ".observe_target("
        0
        "whole-app inferred provider observation"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "->observe_target("
        0
        "whole-app provider observation"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        ".submit("
        0
        "whole-app inferred direct submit"
    )
    # Three sites: the ledger host's command submit and the two
    # SetupDestination submits (the Home paste field and, since BPR-0733,
    # a Recently viewed entry opened by the same path).
    bpr_require_literal_count(
        bpr_async_mutation_source
        "->submit("
        3
        "whole-app pointer submit allowlist"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "ReviewLedgerHost->submit("
        1
        "review-ledger pointer submit allowlist"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "SetupDestination->submit(aValue.Text)"
        1
        "local setup text navigation is not review publication"
    )
    foreach(forbidden IN ITEMS
        "LiveReviewMutationProvider"
        "ReviewMutationCoordinator->publish("
        "provider->observe_target("
        "provider->submit("
        "(*provider)->observe_target("
        "(*provider)->submit("
        "erased->observe_target("
        "erased->submit("
    )
        string(FIND "${bpr_async_mutation_source}" "${forbidden}" found)
        if(NOT found EQUAL -1)
            message(FATAL_ERROR
                "concrete provider bypasses the asynchronous host with an "
                "observe, submit, or publish action: ${forbidden}"
            )
        endif()
    endforeach()
endfunction()

function(bpr_compute_production_mutation_host_digest source_variable out_variable)
    set(bpr_mutation_branch_source "${${source_variable}}")
    string(FIND
        "${bpr_mutation_branch_source}"
        "void compose_review_mutation("
        bpr_mutation_compose_begin
    )
    string(FIND
        "${bpr_mutation_branch_source}"
		"void refresh_review_projection("
        bpr_mutation_compose_end
    )
    if(bpr_mutation_compose_begin EQUAL -1
        OR bpr_mutation_compose_end EQUAL -1
        OR bpr_mutation_compose_end LESS_EQUAL bpr_mutation_compose_begin
    )
        message(FATAL_ERROR
            "review mutation composition boundary is missing for digest lock"
        )
    endif()
    math(EXPR bpr_mutation_compose_length
        "${bpr_mutation_compose_end} - ${bpr_mutation_compose_begin}"
    )
    string(SUBSTRING
        "${bpr_mutation_branch_source}"
        ${bpr_mutation_compose_begin}
        ${bpr_mutation_compose_length}
        bpr_mutation_compose_source
    )
    string(FIND
        "${bpr_mutation_compose_source}"
        "#else"
        bpr_mutation_production_marker
    )
    if(bpr_mutation_production_marker EQUAL -1)
        message(FATAL_ERROR
            "tests-disabled production review mutation branch is missing"
        )
    endif()
    string(LENGTH "#else" bpr_mutation_production_marker_length)
    math(EXPR bpr_mutation_production_begin
        "${bpr_mutation_production_marker} + ${bpr_mutation_production_marker_length}"
    )
    string(FIND
        "${bpr_mutation_compose_source}"
        "#endif"
        bpr_mutation_production_end
    )
    if(bpr_mutation_production_end LESS_EQUAL bpr_mutation_production_begin)
        message(FATAL_ERROR
            "tests-disabled production review mutation branch is malformed"
        )
    endif()
    math(EXPR bpr_mutation_production_length
        "${bpr_mutation_production_end} - ${bpr_mutation_production_begin}"
    )
    string(SUBSTRING
        "${bpr_mutation_compose_source}"
        ${bpr_mutation_production_begin}
        ${bpr_mutation_production_length}
        bpr_mutation_production_source
    )
    # SHA-256 of the exact LF-normalized bytes after the branch's #else and
    # before its #endif. Tokens and horizontal whitespace remain unmodified.
    bpr_lf_normalized_sha256(
        "${bpr_mutation_production_source}"
        bpr_mutation_digest
    )
    set(${out_variable} "${bpr_mutation_digest}" PARENT_SCOPE)
endfunction()

function(bpr_verify_production_mutation_host_digest source_variable)
    bpr_compute_production_mutation_host_digest(
        ${source_variable}
        bpr_mutation_production_digest
    )
    set(bpr_expected_mutation_production_digest
        "a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a"
    )
    if(NOT bpr_mutation_production_digest STREQUAL
        bpr_expected_mutation_production_digest
    )
        message(FATAL_ERROR
            "tests-disabled production compose_review_mutation branch "
            "digest drifted: expected "
            "${bpr_expected_mutation_production_digest}, found "
            "${bpr_mutation_production_digest}"
        )
    endif()
endfunction()

# Step 3 of the reseal procedure at the top of this file. It prints and
# verifies nothing, so no registered test passes this variable.
if(BPR_PRINT_PINNED_DIGESTS)
    if(NOT DEFINED BPR_SOURCE_ROOT OR BPR_SOURCE_ROOT STREQUAL "")
        message(FATAL_ERROR "BPR_PRINT_PINNED_DIGESTS requires BPR_SOURCE_ROOT")
    endif()
    file(READ
        "${BPR_SOURCE_ROOT}/src/app/src/github_application.cpp"
        bpr_print_application_source
    )
    bpr_compute_full_application_digest(
        bpr_print_application_source
        bpr_print_full_digest
    )
    bpr_compute_production_app_manifest_digest(
        "${BPR_SOURCE_ROOT}/src/app"
        bpr_print_manifest_digest
    )
    bpr_compute_production_mutation_host_digest(
        bpr_print_application_source
        bpr_print_mutation_digest
    )
    message("PINNED_DIGEST full_application ${bpr_print_full_digest}")
    message("PINNED_DIGEST production_app_manifest ${bpr_print_manifest_digest}")
    message("PINNED_DIGEST production_mutation_branch ${bpr_print_mutation_digest}")
    return()
endif()

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
    bpr_verify_full_application_digest(bpr_provider_inventory_source)
    bpr_verify_async_mutation_host_inventory(bpr_provider_inventory_source)
    bpr_verify_production_mutation_host_digest(
        bpr_provider_inventory_source
    )
    if(DEFINED BPR_APP_SOURCE_ROOT)
        bpr_verify_app_source_tree("${BPR_APP_SOURCE_ROOT}")
        bpr_verify_replay_operator_surface("${BPR_APP_SOURCE_ROOT}")
    endif()
    message("ASYNC_MUTATION_HOST_INVENTORY_OK")
    return()
endif()

foreach(required IN ITEMS
    BPR_APPLICATION
    BPR_APP_SOURCE
    BPR_ROOT
    BPR_RUN_BUDGET_MS
    BPR_SOURCE_ROOT
)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(READ "${BPR_APP_SOURCE}" bpr_application_source)
bpr_verify_full_application_digest(bpr_application_source)
bpr_verify_async_mutation_host_inventory(bpr_application_source)
bpr_verify_production_mutation_host_digest(bpr_application_source)
bpr_verify_app_source_tree("${BPR_SOURCE_ROOT}/src/app")
bpr_verify_replay_operator_surface("${BPR_SOURCE_ROOT}/src/app")
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
    "ReviewMutationHost->publish("
    "set_selection_sink("
	"refresh_review_projection(aTerminal)"
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
	"void refresh_review_projection("
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
foreach(required_production_host IN ITEMS
    "select_observed_review_mutation("
    "AttendedReviewMutationMode"
    "select_designated_review_mutation("
    "WindowRead->mutation_transport()"
	"adapters::sqlite::make_review_mutation_recovery_store("
    "make_live_review_mutation_provider("
	"std::move(*recovery)"
    "std::move(LiveReviewMutationGrant)"
    "ReviewMutationHost = std::make_unique<"
    "application::review_mutation_application_host>("
)
    string(FIND "${production_mutation_source}" "${required_production_host}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "production review mutation host composition omitted "
            "${required_production_host}"
        )
    endif()
endforeach()
foreach(forbidden_live_action IN ITEMS
    "make_review_mutation_testing_grant("
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
            "production composition bypasses the asynchronous host with "
            "${forbidden_live_action}"
        )
    endif()
endforeach()

# [[BPR-0875]]. The data root moves inside an owned parent, because the build
# tree this course roots in is writable by other principals and the agent
# workspace neighbourhood refuses such a parent.
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")
bpr_private_data_root(BPR_ROOT)

file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${BPR_ROOT}")
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")
bpr_narrow_owner_private("${BPR_ROOT}")
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
    REVIEW_ROUND_TRIP_COMPOSITION_EVIDENCE:11111111111111
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

# The independent live_mutation_isolation CTest owns the exhaustive executable
# and object-symbol scan with its own 300-second bound. This course keeps its
# source-boundary checks and exercises the editor/provider path once.

file(REMOVE_RECURSE "${BPR_ROOT}")
message("REVIEW_ROUND_TRIP_COMPOSITION_CONTRACT_OK")
