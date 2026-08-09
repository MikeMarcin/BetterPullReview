add_test(
    NAME core_dependency_boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${PROJECT_SOURCE_DIR}/cmake/VerifyCoreBoundary.cmake"
)

add_test(
    NAME core_boundary_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/core_boundary_tests.py"
)

add_test(
    NAME architecture_boundaries
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${PROJECT_SOURCE_DIR}/cmake/VerifyArchitectureBoundaries.cmake"
)

add_test(
    NAME architecture_boundary_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/architecture_boundary_tests.py"
)

add_test(
    NAME git_protocol_boundary
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/git_protocol_boundary_tests.py"
)

# The branches this host does not compile. Every other gate here is downstream
# of a compiler, and a compiler that never sees `#elif defined(__linux__)`
# cannot report anything about it. Before BPR-0155 three translation units did
# not compile for Windows or Linux while every local gate stayed green.
add_test(
    NAME conditional_branch_identifiers
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_conditional_branches.py"
        --root
        "${PROJECT_SOURCE_DIR}"
)
set_tests_properties(
    conditional_branch_identifiers
    PROPERTIES
        PASS_REGULAR_EXPRESSION "CONDITIONAL_BRANCH_IDENTIFIERS_OK"
        TIMEOUT 60
)

add_test(
    NAME documentation_structure
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_documentation.py"
        --root
        "${PROJECT_SOURCE_DIR}"
)

add_test(
    NAME documentation_issue_graph_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_documentation_tests.py"
)

add_test(
    NAME github_recorded_fixture_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/recorded_pull_request_fixture_tests.py"
)

add_test(
    NAME attended_review_verdict_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/attended_review_verdict_tests.py"
)

add_test(
    NAME project_source_dump_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/make_project_source_dumps_tests.py"
)

add_test(
    NAME runtime_audit_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_runtime_surface_tests.py"
)

add_test(
    NAME binary_dependency_audit_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/verify_binary_dependencies_tests.py"
)

add_test(
    NAME platform_configuration_contract
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/platform_configuration_tests.py"
)

add_test(
    NAME dependency_modernization_contract
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

add_test(
    NAME github_contract_fixture_security
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/github_contract_fixture_security_tests.py"
)

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

set(bpr_automated_executable_manifest
    "${CMAKE_BINARY_DIR}/_bpr/automated-executables.txt"
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

add_test(
    NAME live_mutation_isolation
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_live_mutation_isolation.py"
        --root
        "${PROJECT_SOURCE_DIR}"
        --manifest
        "${bpr_automated_executable_manifest}"
)
set_tests_properties(
    live_mutation_isolation
    PROPERTIES
        PASS_REGULAR_EXPRESSION "LIVE_MUTATION_ISOLATION_OK"
        TIMEOUT 300
)

add_test(
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
