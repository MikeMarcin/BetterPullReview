if(APPLE OR WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(bpr_native_platform_vault_self_test_result "true")
else()
    set(bpr_native_platform_vault_self_test_result "false")
endif()

add_test(
    NAME application_public_path
    COMMAND
        $<TARGET_FILE:better_pull_review>
        --self-test
)
set_tests_properties(
    application_public_path
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_UNAVAILABLE_OK"
        TIMEOUT 15
)

add_executable(
    better_pull_review_worker_executor_tests
    worker_executor_tests.cpp
)
target_link_libraries(
    better_pull_review_worker_executor_tests
    PRIVATE
        BetterPullReview::Composition
)
target_compile_features(
    better_pull_review_worker_executor_tests
    PRIVATE
        cxx_std_23
)
bpr_enable_hardening(better_pull_review_worker_executor_tests)
bpr_enable_warnings(better_pull_review_worker_executor_tests)

add_test(
    NAME application_worker_executor
    COMMAND better_pull_review_worker_executor_tests
)
set_tests_properties(
    application_worker_executor
    PROPERTIES
        PASS_REGULAR_EXPRESSION "worker-executor-tests:pass"
        TIMEOUT 15
)

add_test(
    NAME application_composition_contract
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/composition_contract_tests.py"
        --cmake
        "${CMAKE_COMMAND}"
        --qt-dir
        "${Qt6_DIR}"
)
set_tests_properties(
    application_composition_contract
    PROPERTIES
        PASS_REGULAR_EXPRESSION "OK"
        TIMEOUT 600
)

set(bpr_public_path_fixture_environment "QT_QPA_PLATFORM=offscreen")
if(UNIX AND NOT APPLE)
    list(APPEND
        bpr_public_path_fixture_environment
        "QT_STYLE_OVERRIDE=Windows"
    )
endif()

add_test(
    NAME application_native_appearance_probe_contract
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:better_pull_review>"
        "-DBPR_BEFORE_RENDER=${CMAKE_CURRENT_BINARY_DIR}/native-appearance-before.png"
        "-DBPR_AFTER_RENDER=${CMAKE_CURRENT_BINARY_DIR}/native-appearance-after.png"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/native_appearance_probe_contract.cmake"
)
set_tests_properties(
    application_native_appearance_probe_contract
    PROPERTIES
        PASS_REGULAR_EXPRESSION "NATIVE_APPEARANCE_PROBE_CONTRACT_OK"
        TIMEOUT 30
)

add_test(
    NAME application_github_public_path_github_com
    COMMAND
        $<TARGET_FILE:better_pull_review>
        --github-fixture
        github-com
        --render-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/github-public-path-github-com.png"
)
set_tests_properties(
    application_github_public_path_github_com
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+GITHUB_FIXTURE_OK:github.com[\r\n]+RENDER_OK"
        TIMEOUT 30
)

add_test(
    NAME application_github_public_path_ghes_3_18
    COMMAND
        $<TARGET_FILE:better_pull_review>
        --github-fixture
        ghes-3.18
        --render-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/github-public-path-ghes-3.18.png"
)
set_tests_properties(
    application_github_public_path_ghes_3_18
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+GITHUB_FIXTURE_OK:ghes-3.18[\r\n]+RENDER_OK"
        TIMEOUT 30
)

add_test(
    NAME binary_dependency_surface
    COMMAND
        "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/scripts/verify_binary_dependencies.py"
        $<TARGET_FILE:better_pull_review>
)
set_tests_properties(
    binary_dependency_surface
    PROPERTIES
        PASS_REGULAR_EXPRESSION "BINARY_DEPENDENCIES_OK"
        TIMEOUT 15
)

add_test(
    NAME runtime_process_surface
    COMMAND
        "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/scripts/verify_runtime_surface.py"
        $<TARGET_FILE:better_pull_review>
)
set_tests_properties(
    runtime_process_surface
    PROPERTIES
        PASS_REGULAR_EXPRESSION "RUNTIME_SURFACE_OK"
        TIMEOUT 15
)

add_test(
    NAME application_render
    COMMAND
        $<TARGET_FILE:better_pull_review>
        --render-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/foundation-window.png"
)
set_tests_properties(
    application_render
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1.25"
        PASS_REGULAR_EXPRESSION "RENDER_OK"
        TIMEOUT 15
)
