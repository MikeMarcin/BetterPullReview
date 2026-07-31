find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network)
find_package(Threads REQUIRED)

add_executable(
    better_pull_review_native_https_transport_tests
    native_https_transport_tests.cpp
)
target_link_libraries(
    better_pull_review_native_https_transport_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        BetterPullReview::Application
        Qt6::Core
        Qt6::Network
        Threads::Threads
)
target_include_directories(
    better_pull_review_native_https_transport_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    better_pull_review_native_https_transport_tests
    PRIVATE
        BPR_NATIVE_HTTPS_TESTING=1
)
bpr_enable_hardening(
    better_pull_review_native_https_transport_tests
)
bpr_enable_warnings(
    better_pull_review_native_https_transport_tests
)

add_test(
    NAME native_https_transport_contract
    COMMAND
        better_pull_review_native_https_transport_tests
        --contract
)
set_tests_properties(
    native_https_transport_contract
    PROPERTIES
        TIMEOUT 30
)

add_executable(
    better_pull_review_native_git_smart_transport_tests
    native_git_smart_transport_tests.cpp
)
target_link_libraries(
    better_pull_review_native_git_smart_transport_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        BetterPullReview::Application
        Qt6::Core
        Qt6::Network
        Threads::Threads
)
target_include_directories(
    better_pull_review_native_git_smart_transport_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    better_pull_review_native_git_smart_transport_tests
    PRIVATE
        BPR_NATIVE_HTTPS_TESTING=1
)
bpr_enable_hardening(
    better_pull_review_native_git_smart_transport_tests
)
bpr_enable_warnings(
    better_pull_review_native_git_smart_transport_tests
)

add_test(
    NAME native_git_smart_transport_contract
    COMMAND better_pull_review_native_git_smart_transport_tests --contract
)
set_tests_properties(
    native_git_smart_transport_contract
    PROPERTIES
        TIMEOUT 30
)

if(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(
        BPR_TEST_NATIVE_HTTPS_OPENSSL
        NAMES openssl
        REQUIRED
    )
    add_test(
        NAME native_https_transport_fixture
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tests/scripts/verify_native_https_transport.py"
            --executable
            "$<TARGET_FILE:better_pull_review_native_https_transport_tests>"
            --openssl
            "${BPR_TEST_NATIVE_HTTPS_OPENSSL}"
    )
    set_tests_properties(
        native_https_transport_fixture
        PROPERTIES
            RESOURCE_LOCK native_https_loopback
            TIMEOUT 90
    )

    add_test(
        NAME native_git_smart_transport_fixture
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tests/github_contract/verify_native_git_smart_transport.py"
            --executable
            "$<TARGET_FILE:better_pull_review_native_git_smart_transport_tests>"
            --openssl
            "${BPR_TEST_NATIVE_HTTPS_OPENSSL}"
    )
    set_tests_properties(
        native_git_smart_transport_fixture
        PROPERTIES
            RESOURCE_LOCK native_https_loopback
            TIMEOUT 90
    )
endif()

add_executable(
    better_pull_review_gh_cli_transport_tests
    gh_cli_transport_tests.cpp
)
target_link_libraries(
    better_pull_review_gh_cli_transport_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        BetterPullReview::Application
)
target_include_directories(
    better_pull_review_gh_cli_transport_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
bpr_enable_hardening(
    better_pull_review_gh_cli_transport_tests
)
bpr_enable_warnings(
    better_pull_review_gh_cli_transport_tests
)

add_test(
    NAME gh_cli_transport
    COMMAND better_pull_review_gh_cli_transport_tests
)
set_tests_properties(
    gh_cli_transport
    PROPERTIES
        TIMEOUT 30
)

add_executable(
    better_pull_review_github_cli_process_runner_tests
    github_cli_process_runner_tests.cpp
)
target_link_libraries(
    better_pull_review_github_cli_process_runner_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        Qt6::Core
        Threads::Threads
)
target_include_directories(
    better_pull_review_github_cli_process_runner_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
bpr_enable_hardening(
    better_pull_review_github_cli_process_runner_tests
)
bpr_enable_warnings(
    better_pull_review_github_cli_process_runner_tests
)

add_test(
    NAME github_cli_process_runner
    COMMAND better_pull_review_github_cli_process_runner_tests
)
set_tests_properties(
    github_cli_process_runner
    PROPERTIES
        TIMEOUT 60
)

add_executable(
    better_pull_review_github_contract_tests
    github_read_contract_tests.cpp
)

target_link_libraries(
    better_pull_review_github_contract_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
)
target_include_directories(
    better_pull_review_github_contract_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_link_libraries(
    better_pull_review_github_contract_tests
    PRIVATE
        Qt6::Core
)
target_compile_definitions(
    better_pull_review_github_contract_tests
    PRIVATE
        BPR_GITHUB_CONTRACT_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures/github/contracts/v1"
)
bpr_enable_hardening(better_pull_review_github_contract_tests)
bpr_enable_warnings(better_pull_review_github_contract_tests)

add_test(
    NAME github_read_contract
    COMMAND better_pull_review_github_contract_tests
)

add_executable(
    better_pull_review_github_paged_read_tests
    github_paged_read_tests.cpp
)
target_link_libraries(
    better_pull_review_github_paged_read_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        BetterPullReview::Application
        Qt6::Core
)
target_include_directories(
    better_pull_review_github_paged_read_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
bpr_enable_hardening(
    better_pull_review_github_paged_read_tests
)
bpr_enable_warnings(
    better_pull_review_github_paged_read_tests
)

add_test(
    NAME github_paged_read
    COMMAND better_pull_review_github_paged_read_tests
)

add_executable(
    better_pull_review_github_provider_tests
    github_provider_tests.cpp
)
target_link_libraries(
    better_pull_review_github_provider_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        BetterPullReview::Application
        Qt6::Core
        Qt6::Network
        Threads::Threads
)
target_include_directories(
    better_pull_review_github_provider_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    better_pull_review_github_provider_tests
    PRIVATE
        BPR_GITHUB_CAPABILITY_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures/github/capabilities/v1"
)
bpr_enable_hardening(
    better_pull_review_github_provider_tests
)
bpr_enable_warnings(
    better_pull_review_github_provider_tests
)

add_test(
    NAME github_provider
    COMMAND better_pull_review_github_provider_tests
)
set_tests_properties(
    github_provider
    PROPERTIES
        TIMEOUT 30
)

add_executable(
    better_pull_review_github_host_profile_tests
    github_host_profile_tests.cpp
)
target_link_libraries(
    better_pull_review_github_host_profile_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
)
target_include_directories(
    better_pull_review_github_host_profile_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
bpr_enable_hardening(better_pull_review_github_host_profile_tests)
bpr_enable_warnings(better_pull_review_github_host_profile_tests)

add_test(
    NAME github_host_profile
    COMMAND better_pull_review_github_host_profile_tests
)

add_executable(
    better_pull_review_github_capability_probe_tests
    github_capability_probe_tests.cpp
)
target_link_libraries(
    better_pull_review_github_capability_probe_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        Qt6::Core
)
target_include_directories(
    better_pull_review_github_capability_probe_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    better_pull_review_github_capability_probe_tests
    PRIVATE
        BPR_GITHUB_CAPABILITY_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures/github/capabilities/v1"
)
bpr_enable_hardening(
    better_pull_review_github_capability_probe_tests
)
bpr_enable_warnings(
    better_pull_review_github_capability_probe_tests
)

add_test(
    NAME github_capability_probe
    COMMAND better_pull_review_github_capability_probe_tests
)

if(APPLE)
    find_library(
        BPR_TEST_MACOS_SECURITY_FRAMEWORK
        Security
        REQUIRED
    )
    find_library(
        BPR_TEST_MACOS_COREFOUNDATION_FRAMEWORK
        CoreFoundation
        REQUIRED
    )

    add_executable(
        better_pull_review_macos_keychain_contract_tests
        macos_keychain_contract_tests.cpp
    )
    target_link_libraries(
        better_pull_review_macos_keychain_contract_tests
        PRIVATE
            BetterPullReview::AdapterGitHub
            BetterPullReview::Application
            "${BPR_TEST_MACOS_SECURITY_FRAMEWORK}"
            "${BPR_TEST_MACOS_COREFOUNDATION_FRAMEWORK}"
    )
    target_include_directories(
        better_pull_review_macos_keychain_contract_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
    )
    bpr_enable_hardening(
        better_pull_review_macos_keychain_contract_tests
    )
    bpr_enable_warnings(
        better_pull_review_macos_keychain_contract_tests
    )

    add_test(
        NAME macos_keychain_contract
        COMMAND better_pull_review_macos_keychain_contract_tests
    )

    add_executable(
        better_pull_review_macos_keychain_native_tests
        macos_keychain_native_tests.cpp
    )
    target_link_libraries(
        better_pull_review_macos_keychain_native_tests
        PRIVATE
            BetterPullReview::AdapterGitHub
            BetterPullReview::Application
            "${BPR_TEST_MACOS_SECURITY_FRAMEWORK}"
            "${BPR_TEST_MACOS_COREFOUNDATION_FRAMEWORK}"
    )
    target_include_directories(
        better_pull_review_macos_keychain_native_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
    )
    bpr_enable_hardening(
        better_pull_review_macos_keychain_native_tests
    )
    bpr_enable_warnings(
        better_pull_review_macos_keychain_native_tests
    )

    add_test(
        NAME macos_keychain_native_setup
        COMMAND
            better_pull_review_macos_keychain_native_tests
            --fixture-setup
    )
    add_test(
        NAME macos_keychain_native
        COMMAND better_pull_review_macos_keychain_native_tests
    )
    add_test(
        NAME macos_keychain_native_cleanup
        COMMAND
            better_pull_review_macos_keychain_native_tests
            --fixture-cleanup
    )
    set_tests_properties(
        macos_keychain_native_setup
        PROPERTIES
            FIXTURES_SETUP macos_keychain_integration_item
            RESOURCE_LOCK macos_keychain_integration_item
            TIMEOUT 30
    )
    set_tests_properties(
        macos_keychain_native
        PROPERTIES
            FIXTURES_REQUIRED macos_keychain_integration_item
            RESOURCE_LOCK macos_keychain_integration_item
            TIMEOUT 30
    )
    set_tests_properties(
        macos_keychain_native_cleanup
        PROPERTIES
            FIXTURES_CLEANUP macos_keychain_integration_item
            RESOURCE_LOCK macos_keychain_integration_item
            TIMEOUT 30
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Threads REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(
        BPR_TEST_LIBSECRET
        REQUIRED
        IMPORTED_TARGET
        libsecret-1>=0.20.0
    )

    add_executable(
        better_pull_review_linux_secret_service_contract_tests
        linux_secret_service_contract_tests.cpp
    )
    target_link_libraries(
        better_pull_review_linux_secret_service_contract_tests
        PRIVATE
            BetterPullReview::AdapterGitHub
            BetterPullReview::Application
            PkgConfig::BPR_TEST_LIBSECRET
            Threads::Threads
    )
    target_include_directories(
        better_pull_review_linux_secret_service_contract_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
    )
    bpr_enable_hardening(
        better_pull_review_linux_secret_service_contract_tests
    )
    bpr_enable_warnings(
        better_pull_review_linux_secret_service_contract_tests
    )

    add_test(
        NAME linux_secret_service_contract
        COMMAND
            better_pull_review_linux_secret_service_contract_tests
    )

    add_executable(
        better_pull_review_linux_secret_service_native_tests
        linux_secret_service_native_tests.cpp
    )
    target_link_libraries(
        better_pull_review_linux_secret_service_native_tests
        PRIVATE
            BetterPullReview::AdapterGitHub
            BetterPullReview::Application
            PkgConfig::BPR_TEST_LIBSECRET
            Threads::Threads
    )
    target_include_directories(
        better_pull_review_linux_secret_service_native_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
    )
    bpr_enable_hardening(
        better_pull_review_linux_secret_service_native_tests
    )
    bpr_enable_warnings(
        better_pull_review_linux_secret_service_native_tests
    )

    add_test(
        NAME linux_secret_service_native_setup
        COMMAND
            better_pull_review_linux_secret_service_native_tests
            --fixture-setup
    )
    add_test(
        NAME linux_secret_service_native
        COMMAND
            better_pull_review_linux_secret_service_native_tests
    )
    add_test(
        NAME linux_secret_service_native_cleanup
        COMMAND
            better_pull_review_linux_secret_service_native_tests
            --fixture-cleanup
    )
    set_tests_properties(
        linux_secret_service_native_setup
        PROPERTIES
            FIXTURES_SETUP linux_secret_service_integration_item
            RESOURCE_LOCK linux_secret_service_integration_item
            TIMEOUT 30
    )
    set_tests_properties(
        linux_secret_service_native
        PROPERTIES
            FIXTURES_REQUIRED linux_secret_service_integration_item
            RESOURCE_LOCK linux_secret_service_integration_item
            TIMEOUT 30
    )
    set_tests_properties(
        linux_secret_service_native_cleanup
        PROPERTIES
            FIXTURES_CLEANUP linux_secret_service_integration_item
            RESOURCE_LOCK linux_secret_service_integration_item
            TIMEOUT 30
    )
endif()

if(WIN32)
    add_executable(
        better_pull_review_windows_credential_manager_contract_tests
        windows_credential_manager_contract_tests.cpp
    )
    target_link_libraries(
        better_pull_review_windows_credential_manager_contract_tests
        PRIVATE
            BetterPullReview::AdapterGitHub
            BetterPullReview::Application
            Advapi32
    )
    target_include_directories(
        better_pull_review_windows_credential_manager_contract_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
    )
    bpr_enable_hardening(
        better_pull_review_windows_credential_manager_contract_tests
    )
    bpr_enable_warnings(
        better_pull_review_windows_credential_manager_contract_tests
    )

    add_test(
        NAME windows_credential_manager_contract
        COMMAND
            better_pull_review_windows_credential_manager_contract_tests
    )

    add_executable(
        better_pull_review_windows_credential_manager_native_tests
        windows_credential_manager_native_tests.cpp
    )
    target_link_libraries(
        better_pull_review_windows_credential_manager_native_tests
        PRIVATE
            BetterPullReview::AdapterGitHub
            BetterPullReview::Application
            Advapi32
            Bcrypt
    )
    target_include_directories(
        better_pull_review_windows_credential_manager_native_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
    )
    bpr_enable_hardening(
        better_pull_review_windows_credential_manager_native_tests
    )
    bpr_enable_warnings(
        better_pull_review_windows_credential_manager_native_tests
    )

    add_test(
        NAME windows_credential_manager_native_setup
        COMMAND
            better_pull_review_windows_credential_manager_native_tests
            --fixture-setup
    )
    add_test(
        NAME windows_credential_manager_native
        COMMAND
            better_pull_review_windows_credential_manager_native_tests
    )
    add_test(
        NAME windows_credential_manager_native_cleanup
        COMMAND
            better_pull_review_windows_credential_manager_native_tests
            --fixture-cleanup
    )
    set_tests_properties(
        windows_credential_manager_native_setup
        PROPERTIES
            FIXTURES_SETUP
                windows_credential_manager_integration_item
            RESOURCE_LOCK
                windows_credential_manager_integration_item
            TIMEOUT 30
    )
    set_tests_properties(
        windows_credential_manager_native
        PROPERTIES
            FIXTURES_REQUIRED
                windows_credential_manager_integration_item
            RESOURCE_LOCK
                windows_credential_manager_integration_item
            TIMEOUT 30
    )
    set_tests_properties(
        windows_credential_manager_native_cleanup
        PROPERTIES
            FIXTURES_CLEANUP
                windows_credential_manager_integration_item
            RESOURCE_LOCK
                windows_credential_manager_integration_item
            TIMEOUT 30
    )
endif()
