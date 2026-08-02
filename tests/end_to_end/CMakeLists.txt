if(APPLE OR WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(bpr_native_platform_vault_self_test_result "true")
else()
    set(bpr_native_platform_vault_self_test_result "false")
endif()

set(bpr_test_qt_root "")
if(DEFINED Qt6_ROOT)
    set(bpr_test_qt_root "${Qt6_ROOT}")
endif()
set(bpr_test_qt_dir "")
if(DEFINED BPR_PRODUCER_QT6_DIR)
    set(bpr_test_qt_dir "${BPR_PRODUCER_QT6_DIR}")
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
            "PUBLIC_PATH_OK[\r\n]+NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_UNAVAILABLE_OK[\r\n]+CLI_UNAVAILABLE_COUNT:1"
        TIMEOUT 15
)

add_test(
    NAME application_public_path_ghes_cli_unavailable
    COMMAND
        $<TARGET_FILE:better_pull_review>
        --self-test
        --ghes-host
        ghe.example.invalid
)
set_tests_properties(
    application_public_path_ghes_cli_unavailable
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_UNAVAILABLE_OK[\r\n]+CLI_UNAVAILABLE_COUNT:2"
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
        Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/composition_contract_tests.py"
        --cmake
        "${CMAKE_COMMAND}"
        --qt-root
        "${bpr_test_qt_root}"
        --qt-dir
        "${bpr_test_qt_dir}"
        --cpm-source-cache
        "${CPM_SOURCE_CACHE}"
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
        TIMEOUT 60
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

foreach(
    fixture_case
    IN ITEMS
        "github-com|github.com"
        "ghes-3.18|ghes-3.18"
)
    string(REPLACE "|" ";" fixture_fields "${fixture_case}")
    list(GET fixture_fields 0 fixture_name)
    list(GET fixture_fields 1 fixture_label)
    string(REPLACE "." "_" fixture_test_name "${fixture_name}")
    add_test(
        NAME application_epoch_persistence_${fixture_test_name}
        COMMAND
            "${CMAKE_COMMAND}"
            "-DBPR_APPLICATION=$<TARGET_FILE:better_pull_review>"
            "-DBPR_FIXTURE=${fixture_name}"
            "-DBPR_FIXTURE_LABEL=${fixture_label}"
            "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/epoch-persistence-${fixture_test_name}"
            -P
            "${CMAKE_CURRENT_SOURCE_DIR}/epoch_persistence_contract.cmake"
    )
    set_tests_properties(
        application_epoch_persistence_${fixture_test_name}
        PROPERTIES
            ENVIRONMENT "${bpr_public_path_fixture_environment}"
            PASS_REGULAR_EXPRESSION
                "EPOCH_PERSISTENCE_CONTRACT_OK:${fixture_label}"
            TIMEOUT 240
    )
endforeach()

add_test(
    NAME application_epoch_maintenance_public_path
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/epoch-maintenance-public-path"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/epoch_maintenance_public_path_contract.cmake"
)
set_tests_properties(
    application_epoch_maintenance_public_path
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "EPOCH_MAINTENANCE_PUBLIC_PATH_CONTRACT_OK"
        TIMEOUT 360
)

add_test(
    NAME binary_dependency_surface
    COMMAND
        Python3::Interpreter
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
        Python3::Interpreter
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
