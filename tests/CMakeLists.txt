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
