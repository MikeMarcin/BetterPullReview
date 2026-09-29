if(BPR_BUILD_TOUR_CLI)
    bpr_add_executable(test_tour_publication_cli application/tour_publication_cli_tests.cpp "${PROJECT_SOURCE_DIR}/src/app/src/review_submission_fixture.cpp")
    target_compile_definitions(test_tour_publication_cli PRIVATE BPR_BUILD_TESTING=1)
    target_include_directories(test_tour_publication_cli PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src")
    target_link_libraries(test_tour_publication_cli PRIVATE BetterPullReview::TourRuntime BetterPullReview::AdapterGitHub BetterPullReview::AdapterSQLite Qt6::Core)
    bpr_enable_hardening(test_tour_publication_cli)
    bpr_enable_warnings(test_tour_publication_cli)
    bpr_add_network_isolated_test(NAME tour_cli_publication COMMAND test_tour_publication_cli)
    bpr_add_network_isolated_test(NAME tour_cli_terminal_publication COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tests/scripts/tour_cli_publication_tests.py" "$<TARGET_FILE:test_tour_publication_cli>")
    bpr_add_executable(test_tour_cli_composition application/tour_cli_tests.cpp)
    target_link_libraries(test_tour_cli_composition PRIVATE BetterPullReview::TourRuntime BetterPullReview::AdapterSQLite Qt6::Core)
    bpr_enable_hardening(test_tour_cli_composition)
    bpr_enable_warnings(test_tour_cli_composition)
    bpr_add_network_isolated_test(NAME tour_cli_composition COMMAND test_tour_cli_composition)
    bpr_add_network_isolated_test(NAME tour_cli_process_commands
        COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tests/scripts/tour_cli_commands_tests.py" "$<TARGET_FILE:test_tour_cli_composition>")
    bpr_add_network_isolated_test(NAME skill_pr_tour_package
        COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tests/scripts/pr_tour_skill_tests.py"
        "${PROJECT_SOURCE_DIR}/packaging/agent-skills/pr-tour" "$<TARGET_FILE:test_tour_cli_composition>")
    bpr_add_network_isolated_test(NAME tour_cli_public_entry
        COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tests/scripts/tour_cli_tests.py" "$<TARGET_FILE:app_bpr>")
endif()

bpr_add_executable(
    test_better_pull_review_designated_live_mutation_gate_tests
    "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_gate.cpp"
    "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_gate_tests.cpp"
)
target_compile_features(
    test_better_pull_review_designated_live_mutation_gate_tests
    PRIVATE
        cxx_std_23
)
target_link_libraries(
    test_better_pull_review_designated_live_mutation_gate_tests
    PRIVATE
        Threads::Threads
)
bpr_enable_hardening(test_better_pull_review_designated_live_mutation_gate_tests)
bpr_enable_warnings(test_better_pull_review_designated_live_mutation_gate_tests)
bpr_add_network_isolated_test(
    NAME designated_live_mutation_gate
    COMMAND test_better_pull_review_designated_live_mutation_gate_tests
)

set(bpr_designated_live_canary_surface_material "")
foreach(
    bpr_designated_live_canary_surface_file
    IN ITEMS
        designated_live_mutation_canary.cpp
        designated_live_mutation_canary.hpp
        designated_live_mutation_child.cpp
        designated_live_mutation_child.hpp
)
    set(
        bpr_designated_live_canary_surface_path
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/${bpr_designated_live_canary_surface_file}"
    )
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${bpr_designated_live_canary_surface_path}"
    )
    file(
        SHA256
        "${bpr_designated_live_canary_surface_path}"
        bpr_designated_live_canary_surface_file_sha256
    )
    string(
        APPEND
        bpr_designated_live_canary_surface_material
        "${bpr_designated_live_canary_surface_file}=${bpr_designated_live_canary_surface_file_sha256}\n"
    )
endforeach()
string(
    SHA256
    BPR_DESIGNATED_LIVE_CANARY_SURFACE_SHA256
    "${bpr_designated_live_canary_surface_material}"
)

if(BPR_BUILD_QT)
    bpr_add_executable(
        tool_better_pull_review_designated_live_mutation_runner
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_canary.cpp"
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_child.cpp"
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_gate.cpp"
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_runner.cpp"
    )
    target_compile_features(
        tool_better_pull_review_designated_live_mutation_runner
        PRIVATE
            cxx_std_23
    )
    target_link_libraries(
        tool_better_pull_review_designated_live_mutation_runner
        PRIVATE
            glaze::glaze
            Qt6::Core
            Qt6::Network
    )
    bpr_enable_hardening(tool_better_pull_review_designated_live_mutation_runner)
    bpr_enable_warnings(tool_better_pull_review_designated_live_mutation_runner)

    bpr_add_executable(
        test_better_pull_review_designated_live_mutation_canary
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_canary.cpp"
    )
    target_compile_features(
        test_better_pull_review_designated_live_mutation_canary
        PRIVATE
            cxx_std_23
    )
    bpr_enable_hardening(test_better_pull_review_designated_live_mutation_canary)
    bpr_enable_warnings(test_better_pull_review_designated_live_mutation_canary)
    target_compile_definitions(
        tool_better_pull_review_designated_live_mutation_runner
        PRIVATE
            BPR_DESIGNATED_LIVE_CANARY_NO_MAIN=1
            BPR_DESIGNATED_LIVE_CANARY_SURFACE_SHA256="${BPR_DESIGNATED_LIVE_CANARY_SURFACE_SHA256}"
    )

    bpr_add_executable(
        test_better_pull_review_designated_live_mutation_runner_digest_mismatch
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_canary.cpp"
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_child.cpp"
        "${PROJECT_SOURCE_DIR}/tests/network_isolation/designated_live_mutation_runner_tests.cpp"
    )
    target_compile_features(
        test_better_pull_review_designated_live_mutation_runner_digest_mismatch
        PRIVATE
            cxx_std_23
    )
    target_compile_definitions(
        test_better_pull_review_designated_live_mutation_runner_digest_mismatch
        PRIVATE
            BPR_DESIGNATED_LIVE_CANARY_NO_MAIN=1
            BPR_DESIGNATED_LIVE_CANARY_SURFACE_SHA256="${BPR_DESIGNATED_LIVE_CANARY_SURFACE_SHA256}"
    )
    target_link_libraries(
        test_better_pull_review_designated_live_mutation_runner_digest_mismatch
        PRIVATE
            glaze::glaze
            Threads::Threads
    )
    bpr_enable_hardening(test_better_pull_review_designated_live_mutation_runner_digest_mismatch)
    bpr_enable_warnings(test_better_pull_review_designated_live_mutation_runner_digest_mismatch)
    bpr_add_network_isolated_test(
        NAME designated_live_mutation_runner_digest_mismatch
        COMMAND test_better_pull_review_designated_live_mutation_runner_digest_mismatch
    )
endif()

bpr_add_designated_live_mutation_test()

bpr_add_network_isolated_test(
    NAME core_dependency_boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${PROJECT_SOURCE_DIR}/cmake/VerifyCoreBoundary.cmake"
)

bpr_add_network_isolated_test(
    NAME core_boundary_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/core_boundary_tests.py"
)

bpr_add_network_isolated_test(
    NAME architecture_boundaries
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${PROJECT_SOURCE_DIR}/cmake/VerifyArchitectureBoundaries.cmake"
)

bpr_add_network_isolated_test(
    NAME architecture_boundary_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/architecture_boundary_tests.py"
)

bpr_add_network_isolated_test(
    NAME git_protocol_boundary
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/git_protocol_boundary_tests.py"
)

# The branches this host does not compile. Every other gate here is downstream
# of a compiler, and a compiler that never sees `#elif defined(__linux__)`
# cannot report anything about it. Before BPR-0155 three translation units did
# not compile for Windows or Linux while every local gate stayed green.
bpr_add_network_isolated_test(
    NAME conditional_branch_identifiers
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_conditional_branches.py"
        --root
        "${PROJECT_SOURCE_DIR}"
)

# The gate above reports on source it cannot compile, so nothing downstream
# catches it being wrong. It read `return _read(x)` as declaring `_read` and
# then, because spelling comparison strips underscores, called the POSIX `read`
# in the opposite branch a rename of it. These cases pin both directions: a real
# rename is still caught, and two correct platform spellings are not.
bpr_add_network_isolated_test(
    NAME conditional_branch_identifier_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/conditional_branch_identifier_tests.py"
)
set_tests_properties(
    conditional_branch_identifiers
    PROPERTIES
        PASS_REGULAR_EXPRESSION "CONDITIONAL_BRANCH_IDENTIFIERS_OK"
        TIMEOUT 60
)

# Only the inventory half runs here. The tests-disabled Release absence scan
# needs a build this graph does not contain, so it stays a closeout command
# documented in docs/development/build-and-test.md.
# BPR-0528: the inventory shells `ctest --show-only`, which needs the
# drive-letter translation a lowbox token is refused, so inside the
# AppContainer CTest lists the Python-driven tests with no command at all;
# it reads a listing and opens no socket, so the exemption costs no network
# claim.
bpr_add_network_isolated_test(
    NAME rfc0017_test_inventory
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_rfc0017_test_inventory.py"
        --build-root
        "${PROJECT_BINARY_DIR}"
        --build-config
        "$<CONFIG>"
)
set_tests_properties(
    rfc0017_test_inventory
    PROPERTIES
        PASS_REGULAR_EXPRESSION "RFC0017_TEST_INVENTORY_OK"
        TIMEOUT 60
)

# The negative control for the absence half. This graph's application is
# tests-enabled and links the `BPR_NATIVE_HTTPS_TESTING` factories, so the
# scan that must pass against a shipped artifact must fail against it, naming
# the test-seam factory. A scan that could pass here would pass against
# anything (BPR-0451). The core-only graph has no application to scan.
if(TARGET app_better_pull_review)
    # BPR-0890. The scan it runs must fail here, so this row's child exits 1 on
    # purpose and the row pins the refusal text below. `EXPECTS_NONZERO_EXIT`
    # says that in the registration rather than leaving the seam's exit-status
    # guard to discover it: the run verdict `PUBLIC_PATH_FAILED` still fails
    # this row, only the non-zero exit does not.
    bpr_add_network_isolated_test(
        NAME rfc0017_release_absence_negative_control
        EXPECTS_NONZERO_EXIT
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/scripts/verify_rfc0017_test_inventory.py"
            --release-binary
            "$<TARGET_FILE:app_better_pull_review>"
    )
    set_tests_properties(
        rfc0017_release_absence_negative_control
        PROPERTIES
            PASS_REGULAR_EXPRESSION
                "symbol present in release binary: make_native_https_credential_free_transport_for_test"
            TIMEOUT 120
    )
endif()

# Same split as above, for the same reason. The inventory half runs in every
# configured graph and anchors itself on whether Qt and the loopback fixture
# were built; the shipped-artifact scan needs a tests-disabled Release build
# this graph does not contain, so it stays a closeout command documented in
# docs/development/build-and-test.md and named in the RFC-BPR-0016 ledger.
# BPR-0528: the inventory shells `ctest --show-only`, which needs the
# drive-letter translation a lowbox token is refused, so inside the
# AppContainer CTest lists the Python-driven tests with no command at all;
# it reads a listing and opens no socket, so the exemption costs no network
# claim.
bpr_add_network_isolated_test(
    NAME rfc0016_test_inventory
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_rfc0016_test_inventory.py"
        --build-dir
        "${PROJECT_BINARY_DIR}"
        --repository-root
        "${PROJECT_SOURCE_DIR}"
        --build-config
        "$<CONFIG>"
)
set_tests_properties(
    rfc0016_test_inventory
    PROPERTIES
        PASS_REGULAR_EXPRESSION "RFC0016_TEST_INVENTORY_OK"
        TIMEOUT 60
)

bpr_add_network_isolated_test(
    NAME documentation_structure
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_documentation.py"
        --root
        "${PROJECT_SOURCE_DIR}"
)

bpr_add_network_isolated_test(
    NAME documentation_issue_graph_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_documentation_tests.py"
)

bpr_add_network_isolated_test(
    NAME package_runtime_contract
    WINDOWS_UNCONTAINED
    COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tests/scripts/package_runtime_tests.py"
)

bpr_add_network_isolated_test(
    NAME release_source_assets_contract
    WINDOWS_UNCONTAINED
    COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tests/scripts/release_source_assets_tests.py"
)

# [[BPR-1020]]. Both audit contracts pass inside the Windows AppContainer (6 and
# 9 tests, measured on the Windows station 2026-09-23), so they run contained on
# every platform.
bpr_add_network_isolated_test(
    NAME release_package_audit_contract
    COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tests/scripts/verify_release_package_tests.py"
)

bpr_add_network_isolated_test(
    NAME release_security_audit_contract
    COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tests/scripts/verify_release_security_tests.py"
)

bpr_add_network_isolated_test(
    NAME issue_id_allocation_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/allocate_issue_id_tests.py"
)

bpr_add_network_isolated_test(
    NAME github_recorded_fixture_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/recorded_pull_request_fixture_tests.py"
)

if(TARGET Git::Git)
    bpr_add_network_isolated_test(
        NAME large_review_fixture_contract
        WINDOWS_UNCONTAINED
        COMMAND Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/large_review_fixture_tests.py"
            --git "$<TARGET_FILE:Git::Git>"
    )
    set_tests_properties(large_review_fixture_contract PROPERTIES
        PASS_REGULAR_EXPRESSION "LARGE_REVIEW_FIXTURE_OK"
        TIMEOUT 90)
endif()

bpr_add_network_isolated_test(
    NAME github_conversation_fixture_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/bpr0417_deep_conversation_fixture_contract_tests.py"
)

bpr_add_network_isolated_test(
    NAME attended_review_verdict_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/attended_review_verdict_tests.py"
)

# The operator-packet validator moved with the rest of the retained RFC-0016
# authority when the shared skills were adopted; its own contract tests moved
# beside it, and this registration named the deleted source until then.
bpr_add_network_isolated_test(
    NAME operator_response_validation_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verification/rfc0016_attended_contract/scripts/test_validate_operator_packet.py"
)

bpr_add_network_isolated_test(
    NAME project_source_dump_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/make_project_source_dumps_tests.py"
)

bpr_add_network_isolated_test(
    NAME runtime_audit_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_runtime_surface_tests.py"
)

bpr_add_network_isolated_test(
    NAME binary_dependency_audit_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_binary_dependencies_tests.py"
)

bpr_add_network_isolated_test(
    NAME composed_host_verifier_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_composed_hosts_tests.py"
)
set_tests_properties(
    composed_host_verifier_contract
    PROPERTIES
        TIMEOUT 60
)

bpr_add_network_isolated_test(
    NAME contained_temporary_usage_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/contained_temporary_usage_tests.py"
)

# [[BPR-1025]]. Which compilers the GCC aggregate-cleanup survey calibrates:
# by each unit's language, every distinct C++ compiler, before any unit is
# surveyed. The compiler-facing steps are replaced, so no compiler is needed.
bpr_add_network_isolated_test(
    NAME gcc_aggregate_survey_selection_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/survey_gcc_aggregate_cleanups_tests.py"
)

# The same helper's own behaviour: it works at the declared interpreter floor
# ([[BPR-0867]]) and names a removal it cannot perform ([[BPR-0868]]). Contained
# on Windows like its neighbour above; nothing in it needs the host.
bpr_add_network_isolated_test(
    NAME contained_temporary_support_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/contained_temporary_support_tests.py"
)

# [[BPR-0871]]. A passing unittest row that skipped a case showed only an "s"
# and a count. skip_report.py names each skipped case and its reason in the
# row's output; this row pins that, and fails when a unittest script under
# tests/ can skip a case without running through it. Contained on Windows: it
# runs in-process and only reads the tree.
bpr_add_network_isolated_test(
    NAME skip_report_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/skip_report_tests.py"
)

bpr_add_network_isolated_test(
    NAME coding_standard_format_gate_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/coding_standard_format_gate_tests.py"
        --cmake
        "${CMAKE_COMMAND}"
)

# [[BPR-0865]]. The repo-wide format gate as a test in the sweep. The row
# above tests the gate script. This row runs the gate on the project, so an
# unformatted file under src/ or tests/ fails the sweep.
#
# It runs the clang-format that configure found (CodingStandardClangFormat.cmake).
# Configure passes over an older tool earlier on PATH. Where no supported tool
# exists, the row reports Skipped: an old or missing tool says nothing about
# whether the source is formatted, so it must not fail the row. A skip still
# appears in the sweep's summary, so it is not mistaken for a pass.
#
# The skip is an exit code, not an output expression. If CTest stopped applying
# SKIP_RETURN_CODE before the inherited TEST_RUNNER_CHILD_EXIT_FAILED error
# expression, the row would fail, which is visible, rather than pass. It is
# applied first on CMake 4.4.2 (measured).
#
# Contained on Windows too. The gate launches clang-format.exe from its install
# directory, and the container allows that. Measured on the Windows station on
# 2026-09-23 ([[BPR-0865]]): contained, it checked all 791 files. It had been
# pinned child-process-denied only by analogy with BPR-0872's rows.
if(BPR_ENABLE_CODING_TARGETS)
    set(bpr_format_check_skip_exit_code 77)
    set(bpr_format_check_tool)
    if(BPR_CLANG_FORMAT_EXECUTABLE)
        set(bpr_format_check_tool --clang-format "${BPR_CLANG_FORMAT_EXECUTABLE}")
    endif()
    bpr_add_network_isolated_test(
        NAME coding_format_check
        COMMAND
            Python3::Interpreter
            "${CODING_STANDARD_FORMAT_CHECK_OR_SKIP_SCRIPT}"
            --root
            "${PROJECT_SOURCE_DIR}"
            --skip-exit-code
            "${bpr_format_check_skip_exit_code}"
            ${bpr_format_check_tool}
    )
    set_tests_properties(
        coding_format_check
        PROPERTIES
            SKIP_RETURN_CODE "${bpr_format_check_skip_exit_code}"
            TIMEOUT 600
    )
endif()

bpr_add_network_isolated_test(
    NAME platform_configuration_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/platform_configuration_tests.py"
)

bpr_add_network_isolated_test(
    NAME application_icon_assets_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/app_icon_assets_tests.py"
)

bpr_add_network_isolated_test(
    NAME dif_assets_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/dif_asset_tests.py"
        --cxx
        "${CMAKE_CXX_COMPILER}"
)

bpr_add_network_isolated_test(
    NAME dependency_modernization_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/dependency_modernization_tests.py"
        --build-dir
        "${CMAKE_BINARY_DIR}"
        --cmake
        "${CMAKE_COMMAND}"
        --cpm-source-cache
        "${CPM_SOURCE_CACHE}"
        --sqlite-source
        "${BPR_SQLITE_SOURCE_DIR}"
)

bpr_add_network_isolated_test(
    NAME github_contract_fixture_security
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/github_contract_fixture_security_tests.py"
)

bpr_add_network_isolated_test(
    NAME rfc0016_device_capture_security
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/rfc0016_device_capture_tests.py"
)

# Retained RFC-0016 authority remains valid after shared-skill cutover.
bpr_add_network_isolated_test(
    NAME rfc0016_relocated_operator_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/rfc0016_relocated_operator_contract_tests.py"
)

# BPR-0312. The gate standing between "the owner approved building public
# preview" and "these four definitions are what gets built". It reaches no
# network and writes only inside a scratch repository it creates.
bpr_add_network_isolated_test(
    NAME rfc0017_public_preview_decision_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/validate_rfc0017_public_preview_decision_tests.py"
)

# The RFC-BPR-0017 evidence-selector inventory's own refusal table, fed
# inventories directly so no build tree or CTest is consulted: a missing or
# duplicated selector, an anchored selector present where its graph does not
# build the anchor, the ledger's --required/--all insistence, an unknown name,
# and a command that lost the argument that makes it the right suite.
bpr_add_network_isolated_test(
    NAME rfc0017_test_inventory_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_rfc0017_test_inventory_tests.py"
)

bpr_add_network_isolated_test(
    NAME rfc0016_device_live_runner_security
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/rfc0016_device_live_runner_tests.py"
)

bpr_add_network_isolated_test(
    NAME rfc0016_production_identity_security
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_rfc0016_production_identity_tests.py"
)

bpr_add_network_isolated_test(
    NAME rfc0016_production_course_security
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/rfc0016_production_course_tests.py"
)

# BPR-0511. The platform course launches the shipped artifact outside CTest,
# so it must carry the Qt runtime directory the registered tests declare.
# BPR-0518. The type-read structured-binding idiom must carry
# [[maybe_unused]], or MSVC's C4189 breaks the Windows lane one slice at a time.
# BPR-0519. std::getenv in a Windows-built unit must carry the scoped C4996
# suppression or a _dupenv_s branch, or MSVC refuses it on the lane alone.
bpr_add_network_isolated_test(
    NAME msvc_secure_crt_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/msvc_secure_crt_tests.py"
)

# BPR-1027. An incremental relink can leave a vtable's new thunk out of the
# Guard CF function table, and a correct program then fails fast on a virtual
# call. /guard:cf links must be complete, in the sources and in this tree's
# generated link lines.
bpr_add_network_isolated_test(
    NAME cfg_complete_link_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/cfg_complete_link_tests.py"
        --build-dir
        "${CMAKE_BINARY_DIR}"
)

bpr_add_network_isolated_test(
    NAME type_read_binding_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/type_read_binding_tests.py"
)

bpr_add_network_isolated_test(
    NAME rfc0016_platform_course_launch_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/rfc0016_platform_course_tests.py"
)

# BPR-0306. The RFC-BPR-0017 courses take an hour and a target-native host, so
# every refusal they make — a selector missing, doubled, or unknown to the
# inventory; a preview stage, request, header, or vault effect that is not what
# the platform promised; a sanitizer diagnostic under a green CTest run — is
# driven here against fabricated listings and fabricated output instead.
bpr_add_network_isolated_test(
    NAME rfc0017_platform_course_launch_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/rfc0017_platform_course_tests.py"
)

bpr_add_network_isolated_test(
    NAME rfc0017_sanitizer_course_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/rfc0017_sanitizer_course_tests.py"
)

bpr_add_network_isolated_test(
    NAME test_network_isolation_structure_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_test_network_isolation_tests.py"
)

if(APPLE)
    bpr_add_network_isolated_test(
        NAME designated_live_macos_capsule_contract
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/designated_live_macos_capsule_tests.py"
    )
endif()

# BPR-0357. The Mach mechanism BPR-0262 fact_3 and fact_4 rest on exists nowhere
# else in the tree, so it is proven on its own before anything is built on it.
# The target links no product library: this is a platform contract, not a
# composition one, and it must stay outside src/app so no pinned digest moves.
if(APPLE)
    bpr_add_executable(
        test_better_pull_review_macos_service_slot_handshake_tests
        platform/macos_service_slot_handshake_tests.cpp
    )
    target_link_libraries(
        test_better_pull_review_macos_service_slot_handshake_tests
        PRIVATE
            "-framework CoreFoundation"
            "-framework Security"
            bsm
    )
    # This target links no product library, so nothing propagates the standard
    # to it the way BetterPullReview::Application does elsewhere.
    target_compile_features(
        test_better_pull_review_macos_service_slot_handshake_tests
        PRIVATE
            cxx_std_23
    )
    bpr_enable_hardening(
        test_better_pull_review_macos_service_slot_handshake_tests
    )
    bpr_enable_warnings(
        test_better_pull_review_macos_service_slot_handshake_tests
    )

    # The linker already applies an ad-hoc signature on arm64, but it sets the
    # linker-signed bit. Re-sign exactly as scripts/designated_live_macos_capsule.py
    # signs the capsule, so the contract answers the question actually asked
    # rather than a friendlier one.
    add_custom_command(
        TARGET test_better_pull_review_macos_service_slot_handshake_tests
        POST_BUILD
        COMMAND
            codesign --force --sign - --timestamp=none
            "$<TARGET_FILE:test_better_pull_review_macos_service_slot_handshake_tests>"
        VERBATIM
        COMMENT "Signing the SERVICE_SLOT contract as the capsule is signed"
    )

    bpr_add_network_isolated_test(
        NAME macos_service_slot_handshake
        COMMAND test_better_pull_review_macos_service_slot_handshake_tests
    )
    set_tests_properties(
        macos_service_slot_handshake
        PROPERTIES
            PASS_REGULAR_EXPRESSION "macos-service-slot-handshake-tests:pass"
    )

    # BPR-0396. The two targets below compile sources out of `src/app/src`, so
    # they belong only to a graph that is allowed to contain application code.
    # `core-only` is not: its whole value is proving the lower tiers stand alone,
    # and a test reaching into `src/app` is part of what that proves. The
    # designated-live capsule these exercise exists only in a Qt application
    # graph in any case, so nothing is lost by scoping them to one.
    if(BPR_BUILD_QT)
        # BPR-0351. The product's own halves of the authenticated launch channel,
        # exercised against each other. This one links the shipped source rather
        # than a copy, so a change to the channel is a change to what is proven.
        bpr_add_executable(
            test_better_pull_review_macos_supervisor_provenance_tests
            platform/macos_supervisor_provenance_tests.cpp
            "${PROJECT_SOURCE_DIR}/src/app/src/designated_live_macos_provenance.cpp"
        )
        target_include_directories(
            test_better_pull_review_macos_supervisor_provenance_tests
            PRIVATE
                "${PROJECT_SOURCE_DIR}/src/app/src"
        )
        target_link_libraries(
            test_better_pull_review_macos_supervisor_provenance_tests
            PRIVATE
                "-framework CoreFoundation"
                "-framework Security"
                bsm
        )
        target_compile_features(
            test_better_pull_review_macos_supervisor_provenance_tests
            PRIVATE
                cxx_std_23
        )
        bpr_enable_hardening(
            test_better_pull_review_macos_supervisor_provenance_tests
        )
        bpr_enable_warnings(
            test_better_pull_review_macos_supervisor_provenance_tests
        )

        # Signed exactly as the capsule signs, so the contract answers the question
        # actually asked rather than a friendlier one.
        add_custom_command(
            TARGET test_better_pull_review_macos_supervisor_provenance_tests
            POST_BUILD
            COMMAND
                codesign --force --sign - --timestamp=none
                "$<TARGET_FILE:test_better_pull_review_macos_supervisor_provenance_tests>"
            VERBATIM
            COMMENT "Signing the provenance contract as the capsule is signed"
        )

        bpr_add_network_isolated_test(
            NAME macos_supervisor_provenance
            COMMAND test_better_pull_review_macos_supervisor_provenance_tests
        )
        set_tests_properties(
            macos_supervisor_provenance
            PROPERTIES
                PASS_REGULAR_EXPRESSION "macos-supervisor-provenance-tests:pass"
        )

        # BPR-0354. The nested check/use window, closed against a retained
        # descriptor. The barrier is a function call rather than a race.
        bpr_add_executable(
            test_better_pull_review_macos_capsule_identity_tests
            platform/macos_capsule_identity_tests.cpp
            "${PROJECT_SOURCE_DIR}/src/app/src/designated_live_macos_capsule_identity.cpp"
        )
        target_include_directories(
            test_better_pull_review_macos_capsule_identity_tests
            PRIVATE
                "${PROJECT_SOURCE_DIR}/src/app/src"
        )
        target_compile_features(
            test_better_pull_review_macos_capsule_identity_tests
            PRIVATE
                cxx_std_23
        )
        bpr_enable_hardening(test_better_pull_review_macos_capsule_identity_tests)
        bpr_enable_warnings(test_better_pull_review_macos_capsule_identity_tests)
        bpr_add_network_isolated_test(
            NAME macos_capsule_identity
            COMMAND test_better_pull_review_macos_capsule_identity_tests
        )
        set_tests_properties(
            macos_capsule_identity
            PROPERTIES
                PASS_REGULAR_EXPRESSION "macos-capsule-identity-tests:pass"
        )

        # BPR-0448. The real static verifier and the production retained-session
        # component are exercised together. Whole-root and inventory pathname
        # substitutions are deterministic phase changes, not timing races.
        bpr_add_executable(
            test_better_pull_review_macos_supervisor_identity_continuity_tests
            platform/macos_supervisor_identity_continuity_tests.cpp
            "${PROJECT_SOURCE_DIR}/src/app/src/designated_live_macos_capsule_identity.cpp"
            "${PROJECT_SOURCE_DIR}/src/app/src/designated_live_macos_supervisor_identity.cpp"
            "${PROJECT_SOURCE_DIR}/src/app/src/designated_live_macos_supervisor_launch.cpp"
        )
        target_include_directories(
            test_better_pull_review_macos_supervisor_identity_continuity_tests
            PRIVATE
                "${PROJECT_SOURCE_DIR}/src/app/src"
        )
        target_compile_features(
            test_better_pull_review_macos_supervisor_identity_continuity_tests
            PRIVATE
                cxx_std_23
        )
        bpr_enable_hardening(
            test_better_pull_review_macos_supervisor_identity_continuity_tests
        )
        bpr_enable_warnings(
            test_better_pull_review_macos_supervisor_identity_continuity_tests
        )
        add_custom_command(
            TARGET
                test_better_pull_review_macos_supervisor_identity_continuity_tests
            POST_BUILD
            COMMAND
                codesign --force --sign - --timestamp=none
                "$<TARGET_FILE:test_better_pull_review_macos_supervisor_identity_continuity_tests>"
            VERBATIM
            COMMENT "Signing the retained supervisor identity contract"
        )
        bpr_add_network_isolated_test(
            NAME macos_supervisor_identity_continuity
            COMMAND
                test_better_pull_review_macos_supervisor_identity_continuity_tests
                "${PROJECT_SOURCE_DIR}/scripts/designated_live_macos_capsule.py"
        )
        set_tests_properties(
            macos_supervisor_identity_continuity
            PROPERTIES
                PASS_REGULAR_EXPRESSION
                    "macos-supervisor-identity-continuity-tests:pass"
                TIMEOUT 60
        )
    endif()
endif()

set(bpr_network_isolation_verifier_command
    Python3::Interpreter
    "${PROJECT_SOURCE_DIR}/scripts/verify_test_network_isolation.py"
    --root
    "${PROJECT_SOURCE_DIR}"
    --build-dir
    "${CMAKE_BINARY_DIR}"
    --ctest
    "${CMAKE_CTEST_COMMAND}"
    --cmake
    "${CMAKE_COMMAND}"
    --python
    "$<TARGET_FILE:Python3::Interpreter>"
    --backend
    "${BPR_AUTOMATED_TEST_ISOLATION_BACKEND}"
)
if(APPLE)
    list(
        APPEND
        bpr_network_isolation_verifier_command
        --profile
        "${BPR_MACOS_TEST_SANDBOX_PROFILE}"
        --macos-runner
        "${BPR_MACOS_TEST_SANDBOX_RUNNER}"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(
        APPEND
        bpr_network_isolation_verifier_command
        --linux-launcher
        "$<TARGET_FILE:tool_better_pull_review_network_sandbox>"
    )
elseif(WIN32)
    list(
        APPEND
        bpr_network_isolation_verifier_command
        --windows-launcher
        "$<TARGET_FILE:tool_better_pull_review_network_sandbox>"
    )
endif()
get_property(bpr_network_isolation_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(bpr_network_isolation_multi_config)
    # `ctest --show-only` lists nothing for a multi-configuration tree until
    # told which configuration, so the gate names the one it runs in.
    list(
        APPEND
        bpr_network_isolation_verifier_command
        --build-config
        "$<CONFIG>"
    )
endif()
if(BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST)
    list(
        APPEND
        bpr_network_isolation_verifier_command
        --live-image
        "${BPR_DESIGNATED_LIVE_MUTATION_IMAGE}"
        --live-image-sha256
        "${BPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256}"
        --live-mount-parent
        "${BPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT}"
        --live-build-dir
        "${BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR}"
        --live-approval
        "${BPR_DESIGNATED_LIVE_MUTATION_APPROVAL}"
    )
    if(NOT "${BPR_DESIGNATED_LIVE_MUTATION_PROFILE}" STREQUAL "")
        list(APPEND bpr_network_isolation_verifier_command --live-profile "${BPR_DESIGNATED_LIVE_MUTATION_PROFILE}")
    endif()
endif()
bpr_add_network_isolated_test(
    NAME test_network_isolation_manifest
    COMMAND ${bpr_network_isolation_verifier_command}
)
set_tests_properties(
    test_network_isolation_manifest
    PROPERTIES
        TIMEOUT 60
)

foreach(bpr_network_probe IN ITEMS external child)
    bpr_add_network_isolated_test(
        NAME "test_network_isolation_${bpr_network_probe}"
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/network_isolation_counterexamples.py"
            "${bpr_network_probe}"
            "${BPR_AUTOMATED_TEST_ISOLATION_BACKEND}"
    )
endforeach()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # The launcher's user namespace must buy a network namespace and nothing
    # else. Without this, a future launcher could map the caller to 0 again and
    # every permission assertion in the suite would silently stop testing.
    bpr_add_network_isolated_test(
        NAME test_network_isolation_identity
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/network_isolation_counterexamples.py"
            identity
            "${BPR_AUTOMATED_TEST_ISOLATION_BACKEND}"
    )
    set_tests_properties(
        test_network_isolation_identity
        PROPERTIES
            TIMEOUT 15
    )
    # [[BPR-1113]]. Preloaded into nested launcher runs by the row below, so
    # it can reach the launcher's failure paths. It is a test tool only and
    # is never installed.
    add_library(
        better_pull_review_launcher_fault_shim
        MODULE
            "${PROJECT_SOURCE_DIR}/tests/network_isolation/linux_launcher_fault_shim.cpp"
    )
    target_compile_features(
        better_pull_review_launcher_fault_shim
        PRIVATE
            cxx_std_23
    )
    target_link_libraries(
        better_pull_review_launcher_fault_shim
        PRIVATE
            ${CMAKE_DL_LIBS}
    )
    bpr_enable_hardening(better_pull_review_launcher_fault_shim)
    bpr_enable_warnings(better_pull_review_launcher_fault_shim)
    # [[BPR-0906]]. The launcher gives each row its own runtime directory and
    # removes it afterwards, so an owner that a row ends with a signal leaves
    # its activation socket there rather than in the user's /run/user/<uid>.
    # The row runs the launcher once more around such an owner, beside a live
    # endpoint the cleanup must not reach and links that lead out to it
    # ([[BPR-1113]]). Then it runs the launcher with the shim above, into
    # each failure path after the directory exists.
    bpr_add_network_isolated_test(
        NAME test_network_isolation_runtime_directory
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/network_isolation_counterexamples.py"
            runtime
            "${BPR_AUTOMATED_TEST_ISOLATION_BACKEND}"
            "$<TARGET_FILE:tool_better_pull_review_network_sandbox>"
            "$<TARGET_FILE:better_pull_review_launcher_fault_shim>"
    )
    set_tests_properties(
        test_network_isolation_runtime_directory
        PROPERTIES
            TIMEOUT 60
    )
endif()
bpr_add_network_isolated_test(
    NAME test_network_isolation_loopback
    LOOPBACK_PORT_COUNT 1
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/network_isolation_counterexamples.py"
        loopback
        "${BPR_AUTOMATED_TEST_ISOLATION_BACKEND}"
)
if(WIN32)
    # [[BPR-1072]]. Every contained Windows row enters the one AppContainer
    # profile through the launcher, several at once under `ctest -j`. This row
    # starts four launchers at the same moment for eight rounds and requires
    # every child to report the container's token. It runs the launcher itself,
    # so it cannot run inside the container.
    bpr_add_network_isolated_test(
        NAME test_network_isolation_concurrent_launch
        WINDOWS_UNCONTAINED
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/network_isolation_counterexamples.py"
            concurrent
            "${BPR_AUTOMATED_TEST_ISOLATION_BACKEND}"
            "$<TARGET_FILE:tool_better_pull_review_network_sandbox>"
    )
    set_tests_properties(
        test_network_isolation_concurrent_launch
        PROPERTIES
            PASS_REGULAR_EXPRESSION "NETWORK_ISOLATION_CONCURRENT_LAUNCHES_CONTAINED:32"
            TIMEOUT 180
    )
endif()
if(APPLE)
    bpr_add_network_isolated_test(
        NAME test_network_isolation_unrelated
        LOOPBACK_PORT_COUNT 1
        UNRELATED_ENDPOINT_PROBE
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/network_isolation_counterexamples.py"
            unrelated
            "${BPR_AUTOMATED_TEST_ISOLATION_BACKEND}"
    )
endif()
if(TARGET app_better_pull_review)
    bpr_add_network_isolated_test(
        NAME test_network_isolation_application
        COMMAND
            app_better_pull_review
            --network-isolation-probe
    )
endif()
set_tests_properties(
    test_network_isolation_external
    PROPERTIES
        TIMEOUT 15
)
set_tests_properties(
    test_network_isolation_loopback
    PROPERTIES
        TIMEOUT 15
)
if(APPLE)
    set_tests_properties(
        test_network_isolation_unrelated
        PROPERTIES
            TIMEOUT 15
    )
endif()
set_tests_properties(
    test_network_isolation_child
    PROPERTIES
        TIMEOUT 15
)
if(TEST test_network_isolation_application)
    set_tests_properties(
        test_network_isolation_application
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            TIMEOUT 15
    )
endif()

add_custom_target(
    check_boundaries
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${PROJECT_SOURCE_DIR}/cmake/VerifyCoreBoundary.cmake"
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${PROJECT_SOURCE_DIR}/cmake/VerifyArchitectureBoundaries.cmake"
    VERBATIM
)

add_subdirectory(core)
add_subdirectory(application)
add_subdirectory(contract)
add_subdirectory(sqlite_contract)

if(BPR_BUILD_QT)
    add_subdirectory(adapters_qt)
    add_subdirectory(github_contract)
    add_subdirectory(git_contract)
    add_subdirectory(syntax_contract)
    add_subdirectory(end_to_end)
endif()

# Deliberately last: every `bpr_add_executable` call in the tree must already
# have run, or the manifest describes a subset of the binaries and the
# isolation guard passes by not looking. The list is smaller under `core-only`
# because the Qt suites above are not added there, which is correct - each
# configuration proves the property for the binaries it actually built.
get_property(bpr_automated_executables GLOBAL PROPERTY BPR_AUTOMATED_EXECUTABLES)
if(NOT bpr_automated_executables)
    message(
        FATAL_ERROR
        "No non-production executables were registered. Either bpr_add_executable "
        "stopped collecting them or this block moved above the test "
        "subdirectories; in both cases live_mutation_isolation would pass "
        "against an empty manifest."
    )
endif()
list(LENGTH bpr_automated_executables bpr_automated_executable_count)
message(
    STATUS
    "bpr: live_mutation_isolation covers ${bpr_automated_executable_count} "
    "non-production executables"
)

# The manifest embeds $<TARGET_FILE:...>, whose value differs per
# configuration. A multi-config generator such as Visual Studio evaluates this
# file once per configuration, so a single fixed path would be written
# repeatedly with different content and generation fails. Keying the path by
# configuration is correct for single-config generators too, where $<CONFIG>
# is simply the one build type.
set(bpr_automated_executable_manifest
    "${CMAKE_BINARY_DIR}/_bpr/$<CONFIG>/automated-executables.txt"
)
set(bpr_automated_executable_files "")
foreach(bpr_automated_target IN LISTS bpr_automated_executables)
    string(
        APPEND
        bpr_automated_executable_files
        "$<TARGET_FILE:${bpr_automated_target}>\n"
    )
endforeach()
file(
    GENERATE
    OUTPUT "${bpr_automated_executable_manifest}"
    CONTENT "${bpr_automated_executable_files}"
)

# The MSVC linker consumes the COFF symbol table, so `dumpbin /symbols` reports
# only a section summary for a linked image and the binary half of the guard
# would be reading an empty table. The objects that composed each binary do
# carry the table, so record them and let the verifier read those on Windows.
# Written on every platform so the two sides cannot drift apart unnoticed, and
# keyed by binary so the guard keeps its per-binary granularity: the claim is
# that a *non-production* binary carries no live provider, not that the tree
# contains none anywhere.
set(bpr_automated_executable_object_manifest
    "${CMAKE_BINARY_DIR}/_bpr/$<CONFIG>/automated-executable-objects.txt"
)
set(bpr_automated_executable_objects "")
foreach(bpr_automated_target IN LISTS bpr_automated_executables)
    string(
        APPEND
        bpr_automated_executable_objects
        "$<TARGET_FILE:${bpr_automated_target}>"
        "|"
        "$<JOIN:$<TARGET_OBJECTS:${bpr_automated_target}>,|>"
        "\n"
    )
endforeach()
file(
    GENERATE
    OUTPUT "${bpr_automated_executable_object_manifest}"
    CONTENT "${bpr_automated_executable_objects}"
)

bpr_add_network_isolated_test(
    NAME live_mutation_isolation
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_live_mutation_isolation.py"
        --root
        "${PROJECT_SOURCE_DIR}"
        --manifest
        "${bpr_automated_executable_manifest}"
        --object-manifest
        "${bpr_automated_executable_object_manifest}"
)
set_tests_properties(
    live_mutation_isolation
    PROPERTIES
        TIMEOUT 300
)

bpr_add_network_isolated_test(
    NAME live_mutation_isolation_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_live_mutation_isolation_tests.py"
)
# No PASS_REGULAR_EXPRESSION. This is a `unittest` suite and its exit code is
# already the verdict; setting the property makes CTest ignore that exit code
# and pass on a substring match instead. "OK" is satisfied by any incidental
# `..._OK` marker in captured output, which is how a failing suite reported
# Passed.
set_tests_properties(
    live_mutation_isolation_contract
    PROPERTIES
        TIMEOUT 60
)

add_subdirectory(logging)
