bpr_add_executable(test_release_update_transport release_update_transport_tests.cpp)
target_link_libraries(test_release_update_transport PRIVATE BetterPullReview::AdapterGitHub Qt6::Core)
target_include_directories(test_release_update_transport PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src")
target_compile_definitions(test_release_update_transport PRIVATE BPR_NATIVE_HTTPS_TESTING=1)
bpr_enable_hardening(test_release_update_transport)
bpr_enable_warnings(test_release_update_transport)
bpr_add_network_isolated_test(NAME release_update_transport_contract COMMAND test_release_update_transport)
find_program(BPR_UPDATE_TEST_OPENSSL NAMES openssl REQUIRED)
# [[BPR-1647]]. The complete course passes directly; contained MSYS OpenSSL
# cannot create its BaseNamedObjects runtime directory (0xC0000022).
bpr_add_network_isolated_test(NAME release_update_native_https WINDOWS_UNCONTAINED LOOPBACK_PORT_COUNT 1
    COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tests/scripts/verify_release_update_transport.py"
    --executable "$<TARGET_FILE:test_release_update_transport>" --openssl "${BPR_UPDATE_TEST_OPENSSL}")
set_tests_properties(release_update_native_https PROPERTIES TIMEOUT 90)
bpr_add_network_isolated_test(NAME release_update_https_fixture_contract
    COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tests/scripts/release_update_https_fixture_tests.py")

bpr_add_executable(
    test_better_pull_review_native_https_transport_tests
    native_https_transport_tests.cpp
)

if(APPLE)
    bpr_add_executable(
        test_better_pull_review_designated_live_public_observer_tests
        designated_live_public_observer_tests.cpp
    )
    target_link_libraries(
        test_better_pull_review_designated_live_public_observer_tests
        PRIVATE
            BetterPullReview::DesignatedLivePublicObserver
    )
    target_compile_definitions(
        test_better_pull_review_designated_live_public_observer_tests
        PRIVATE
            BPR_DESIGNATED_LIVE_PUBLIC_OBSERVER_TESTING=1
    )
    target_include_directories(
        test_better_pull_review_designated_live_public_observer_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
    )
    bpr_enable_hardening(
        test_better_pull_review_designated_live_public_observer_tests
    )
    bpr_enable_warnings(
        test_better_pull_review_designated_live_public_observer_tests
    )
    find_program(
        BPR_TEST_DESIGNATED_LIVE_OPENSSL
        NAMES openssl
        REQUIRED
    )
    bpr_add_network_isolated_test(
        NAME designated_live_public_observer_fixture
        LOOPBACK_PORT_COUNT 1
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/verify_designated_live_public_observer.py"
            --executable
            "$<TARGET_FILE:test_better_pull_review_designated_live_public_observer_tests>"
            --openssl
            "${BPR_TEST_DESIGNATED_LIVE_OPENSSL}"
    )
    set_tests_properties(
        designated_live_public_observer_fixture
        PROPERTIES
            RESOURCE_LOCK designated_live_public_observer_loopback
            TIMEOUT 90
    )
endif()
target_link_libraries(
    test_better_pull_review_native_https_transport_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        BetterPullReview::Application
        Qt6::Core
        Qt6::Network
        Threads::Threads
)
target_include_directories(
    test_better_pull_review_native_https_transport_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    test_better_pull_review_native_https_transport_tests
    PRIVATE
        BPR_NATIVE_HTTPS_TESTING=1
)
bpr_enable_hardening(
    test_better_pull_review_native_https_transport_tests
)
bpr_enable_warnings(
    test_better_pull_review_native_https_transport_tests
)

bpr_add_network_isolated_test(
    NAME native_https_transport_contract
    COMMAND
        test_better_pull_review_native_https_transport_tests
        --contract
)
set_tests_properties(
    native_https_transport_contract
    PROPERTIES
        TIMEOUT 30
)

bpr_add_executable(
    test_better_pull_review_native_git_smart_transport_tests
    native_git_smart_transport_tests.cpp
)
target_link_libraries(
    test_better_pull_review_native_git_smart_transport_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        BetterPullReview::Application
        Qt6::Core
        Qt6::Network
        Threads::Threads
)
target_include_directories(
    test_better_pull_review_native_git_smart_transport_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    test_better_pull_review_native_git_smart_transport_tests
    PRIVATE
        BPR_NATIVE_HTTPS_TESTING=1
)
bpr_enable_hardening(
    test_better_pull_review_native_git_smart_transport_tests
)
bpr_enable_warnings(
    test_better_pull_review_native_git_smart_transport_tests
)

bpr_add_network_isolated_test(
    NAME native_git_smart_transport_contract
    COMMAND test_better_pull_review_native_git_smart_transport_tests --contract
)
set_tests_properties(
    native_git_smart_transport_contract
    PROPERTIES
        TIMEOUT 30
)

bpr_add_executable(
    test_better_pull_review_native_https_device_authorization_tests
    native_https_device_authorization_tests.cpp
)
target_link_libraries(
    test_better_pull_review_native_https_device_authorization_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        BetterPullReview::Application
        Qt6::Core
        Qt6::Network
        Threads::Threads
)
target_include_directories(
    test_better_pull_review_native_https_device_authorization_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    test_better_pull_review_native_https_device_authorization_tests
    PRIVATE
        BPR_NATIVE_HTTPS_TESTING=1
)
bpr_enable_hardening(
    test_better_pull_review_native_https_device_authorization_tests
)
bpr_enable_warnings(
    test_better_pull_review_native_https_device_authorization_tests
)

bpr_add_network_isolated_test(
    NAME native_https_device_authorization_contract
    COMMAND
        test_better_pull_review_native_https_device_authorization_tests
        --contract
)
set_tests_properties(
    native_https_device_authorization_contract
    PROPERTIES
        TIMEOUT 30
)

# RFC-BPR-0017's anonymous read authority is asked its whole question without a
# network: the binding's match table, the surfaces that refuse it, and the
# refusals the factory makes before a transport exists. It runs on every Qt
# graph because none of it needs a loopback fixture.
bpr_add_executable(
    test_better_pull_review_native_https_anonymous_read_tests
    native_https_anonymous_read_tests.cpp
)
target_link_libraries(
    test_better_pull_review_native_https_anonymous_read_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        BetterPullReview::Application
        Qt6::Core
        Qt6::Network
        Threads::Threads
)
target_include_directories(
    test_better_pull_review_native_https_anonymous_read_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    test_better_pull_review_native_https_anonymous_read_tests
    PRIVATE
        BPR_NATIVE_HTTPS_TESTING=1
)
bpr_enable_hardening(
    test_better_pull_review_native_https_anonymous_read_tests
)
bpr_enable_warnings(
    test_better_pull_review_native_https_anonymous_read_tests
)

bpr_add_network_isolated_test(
    NAME native_https_anonymous_read_contract
    COMMAND
        test_better_pull_review_native_https_anonymous_read_tests
        --contract
)
set_tests_properties(
    native_https_anonymous_read_contract
    PROPERTIES
        TIMEOUT 60
)

if(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(
        BPR_TEST_NATIVE_HTTPS_OPENSSL
        NAMES openssl
        REQUIRED
    )
    bpr_add_network_isolated_test(
        NAME native_https_transport_fixture
        LOOPBACK_PORT_COUNT 2
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/verify_native_https_transport.py"
            --executable
            "$<TARGET_FILE:test_better_pull_review_native_https_transport_tests>"
            --openssl
            "${BPR_TEST_NATIVE_HTTPS_OPENSSL}"
    )
    set_tests_properties(
        native_https_transport_fixture
        PROPERTIES
            RESOURCE_LOCK native_https_loopback
            TIMEOUT 90
    )

    # RFC-BPR-0017's credential-free demonstration is registered separately so
    # verify_rfc0017_test_inventory.py can count exactly one selector for it.
    bpr_add_network_isolated_test(
        NAME rfc0017_credential_free_native_https
        LOOPBACK_PORT_COUNT 2
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/verify_native_https_transport.py"
            --executable
            "$<TARGET_FILE:test_better_pull_review_native_https_transport_tests>"
            --openssl
            "${BPR_TEST_NATIVE_HTTPS_OPENSSL}"
            --suite
            rfc0017
    )
    set_tests_properties(
        rfc0017_credential_free_native_https
        PROPERTIES
            RESOURCE_LOCK native_https_loopback
            TIMEOUT 90
            PASS_REGULAR_EXPRESSION
                "rfc0017-credential-free-native-https:pass"
    )

    # RFC-BPR-0017's anonymous read runs against the same loopback CONNECT and
    # TLS fixture, on the GitHub.com authority the anonymous factory admits. It
    # carries its own selector because the pass line is the evidence: a REST
    # read served with no authorization header, and a canary course through the
    # same fixture proving the header check is live.
    bpr_add_network_isolated_test(
        NAME native_https_anonymous_read_fixture
        LOOPBACK_PORT_COUNT 2
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/verify_native_https_transport.py"
            --executable
            "$<TARGET_FILE:test_better_pull_review_native_https_transport_tests>"
            --openssl
            "${BPR_TEST_NATIVE_HTTPS_OPENSSL}"
            --suite
            anonymous_read
    )
    set_tests_properties(
        native_https_anonymous_read_fixture
        PROPERTIES
            RESOURCE_LOCK native_https_loopback
            TIMEOUT 90
            PASS_REGULAR_EXPRESSION
                "native-https-anonymous-read-fixture:pass"
    )

    bpr_add_network_isolated_test(
        NAME native_git_smart_transport_fixture
        LOOPBACK_PORT_COUNT 1
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/github_contract/verify_native_git_smart_transport.py"
            --executable
            "$<TARGET_FILE:test_better_pull_review_native_git_smart_transport_tests>"
            --openssl
            "${BPR_TEST_NATIVE_HTTPS_OPENSSL}"
    )
    set_tests_properties(
        native_git_smart_transport_fixture
        PROPERTIES
            RESOURCE_LOCK native_https_loopback
            TIMEOUT 90
    )

    bpr_add_network_isolated_test(
        NAME native_https_device_authorization_fixture
        LOOPBACK_PORT_COUNT 1
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/github_contract/verify_native_https_device_authorization.py"
            --executable
            "$<TARGET_FILE:test_better_pull_review_native_https_device_authorization_tests>"
            --openssl
            "${BPR_TEST_NATIVE_HTTPS_OPENSSL}"
    )
    set_tests_properties(
        native_https_device_authorization_fixture
        PROPERTIES
            RESOURCE_LOCK native_https_loopback
            TIMEOUT 90
            PASS_REGULAR_EXPRESSION
                "native-https-device-authorization:pass"
    )

    # BPR-0450 registers the renewal courses under their own selector because
    # the RFC-BPR-0016 final command ledger allocates that exact name and
    # verify_rfc0016_test_inventory.py must be able to count one selector for
    # it. The suite runs the same fixture with the refresh scenarios only,
    # including the lifecycle course that drives the real application host
    # through the real production adapter.
    bpr_add_network_isolated_test(
        NAME github_device_authorization_refresh_contract
        LOOPBACK_PORT_COUNT 1
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/github_contract/verify_native_https_device_authorization.py"
            --executable
            "$<TARGET_FILE:test_better_pull_review_native_https_device_authorization_tests>"
            --openssl
            "${BPR_TEST_NATIVE_HTTPS_OPENSSL}"
            --suite
            refresh
    )
    set_tests_properties(
        github_device_authorization_refresh_contract
        PROPERTIES
            RESOURCE_LOCK native_https_loopback
            TIMEOUT 90
            PASS_REGULAR_EXPRESSION
                "github-device-authorization-refresh:pass"
    )
endif()

if(WIN32)
    # The wire fixture's only extra dependency is an openssl binary for
    # certificate generation; Windows does not put one on PATH, but Git for
    # Windows -- already a hard dependency of the product on this platform --
    # ships OpenSSL. The APPLE/Linux registration above is deliberately
    # untouched, and RFC-BPR-0017's credential-free selector deliberately
    # stays unregistered here (BPR-0446).
    find_program(
        BPR_TEST_NATIVE_HTTPS_OPENSSL
        NAMES openssl
        HINTS
            "$ENV{ProgramFiles}/Git/mingw64/bin"
            "$ENV{ProgramFiles}/Git/usr/bin"
        REQUIRED
    )
    # Uncontained for the measured dos-volume-name cause: the driver's
    # strict executable resolution and CPython's own path initialization both
    # need the drive-letter translation a lowbox token is refused (BPR-0362,
    # BPR-0446). The driver only ever talks to its own loopback origin.
    bpr_add_network_isolated_test(
        NAME native_https_transport_fixture
        WINDOWS_UNCONTAINED
        LOOPBACK_PORT_COUNT 2
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/verify_native_https_transport.py"
            --executable
            "$<TARGET_FILE:test_better_pull_review_native_https_transport_tests>"
            --openssl
            "${BPR_TEST_NATIVE_HTTPS_OPENSSL}"
    )
    set_tests_properties(
        native_https_transport_fixture
        PROPERTIES
            RESOURCE_LOCK native_https_loopback
            TIMEOUT 90
    )

    # BPR-0525 mirrors the two RFC-BPR-0016 device-authorization selectors the
    # APPLE/Linux block above registers and BPR-0446 did not. The guard there
    # exists for the openssl binary this block has already found, and the
    # executable both drive is registered on Windows unguarded as
    # `native_https_device_authorization_contract`, so nothing but the omission
    # kept them off this platform. Arguments are the POSIX ones verbatim.
    #
    # Uncontained for the same dos-volume-name cause as the wire fixture
    # above, measured here rather than inherited: registered contained
    # first, both selectors refused at once with
    # `fail:unavailable-executable` -- the driver's strict executable
    # resolution needs the drive-letter translation a lowbox token is
    # refused (BPR-0362, BPR-0525). Neither reaches a network it is not
    # entitled to; both talk only to their own loopback origin, exactly as
    # their POSIX registrations do.
    bpr_add_network_isolated_test(
        NAME native_https_device_authorization_fixture
        WINDOWS_UNCONTAINED
        LOOPBACK_PORT_COUNT 1
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/github_contract/verify_native_https_device_authorization.py"
            --executable
            "$<TARGET_FILE:test_better_pull_review_native_https_device_authorization_tests>"
            --openssl
            "${BPR_TEST_NATIVE_HTTPS_OPENSSL}"
    )
    set_tests_properties(
        native_https_device_authorization_fixture
        PROPERTIES
            RESOURCE_LOCK native_https_loopback
            TIMEOUT 90
            PASS_REGULAR_EXPRESSION
                "native-https-device-authorization:pass"
    )

    # BPR-0450's separate selector for the renewal courses, mirrored here for
    # the same reason: the RFC-BPR-0016 ledger allocates this exact name and
    # verify_rfc0016_test_inventory.py must be able to count one selector for
    # it on every platform the course audits.
    bpr_add_network_isolated_test(
        NAME github_device_authorization_refresh_contract
        WINDOWS_UNCONTAINED
        LOOPBACK_PORT_COUNT 1
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/github_contract/verify_native_https_device_authorization.py"
            --executable
            "$<TARGET_FILE:test_better_pull_review_native_https_device_authorization_tests>"
            --openssl
            "${BPR_TEST_NATIVE_HTTPS_OPENSSL}"
            --suite
            refresh
    )
    set_tests_properties(
        github_device_authorization_refresh_contract
        PROPERTIES
            RESOURCE_LOCK native_https_loopback
            TIMEOUT 90
            PASS_REGULAR_EXPRESSION
                "github-device-authorization-refresh:pass"
    )
endif()

bpr_add_executable(
    test_better_pull_review_gh_cli_transport_tests
    gh_cli_transport_tests.cpp
)
target_link_libraries(
    test_better_pull_review_gh_cli_transport_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        # [[BPR-PENDING-AX]]. The composed reply course keeps its journal in
        # the shipped SQLite recovery store.
        BetterPullReview::AdapterSQLite
        BetterPullReview::Application
)
target_include_directories(
    test_better_pull_review_gh_cli_transport_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    test_better_pull_review_gh_cli_transport_tests
    PRIVATE BPR_APPLICATION_TESTING=1
)
bpr_enable_hardening(
    test_better_pull_review_gh_cli_transport_tests
)
bpr_enable_warnings(
    test_better_pull_review_gh_cli_transport_tests
)

bpr_add_network_isolated_test(
    NAME gh_cli_transport
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_gh_cli_transport_tests
)
set_tests_properties(
    gh_cli_transport
    PROPERTIES
        TIMEOUT 30
)

bpr_add_executable(
    test_better_pull_review_github_cli_process_runner_tests
    github_cli_process_runner_tests.cpp
)
target_link_libraries(
    test_better_pull_review_github_cli_process_runner_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        Qt6::Core
        Threads::Threads
)
target_include_directories(
    test_better_pull_review_github_cli_process_runner_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
if(WIN32)
    # The test is its own helper child, and the runner's Windows contract
    # hands a child an empty PATH: the only place the loader can then find
    # the shared Qt this executable links is beside the executable, so its
    # runtime DLLs are copied there after each link (BPR-0438). The copy
    # must land in a directory this test owns: in the shared output
    # directory, loader app-directory precedence would hand every sibling
    # a plugin-less Qt, which is how the wire fixture lost its Schannel
    # backend the first time the whole graph built (BPR-0464).
    set_target_properties(
        test_better_pull_review_github_cli_process_runner_tests
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY
                "${CMAKE_CURRENT_BINARY_DIR}/cli_process_runner"
    )
    add_custom_command(
        TARGET test_better_pull_review_github_cli_process_runner_tests
        POST_BUILD
        COMMAND
            "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:test_better_pull_review_github_cli_process_runner_tests>
            $<TARGET_FILE_DIR:test_better_pull_review_github_cli_process_runner_tests>
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
endif()
bpr_enable_hardening(
    test_better_pull_review_github_cli_process_runner_tests
)
bpr_enable_warnings(
    test_better_pull_review_github_cli_process_runner_tests
)

bpr_add_network_isolated_test(
    NAME github_cli_process_runner
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_github_cli_process_runner_tests
)
set_tests_properties(
    github_cli_process_runner
    PROPERTIES
        TIMEOUT 60
)

bpr_add_executable(
    test_better_pull_review_github_contract_tests
    github_read_contract_tests.cpp
)

target_link_libraries(
    test_better_pull_review_github_contract_tests
    PRIVATE
        glaze::glaze
        BetterPullReview::AdapterGitHub
)
target_include_directories(
    test_better_pull_review_github_contract_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_link_libraries(
    test_better_pull_review_github_contract_tests
    PRIVATE
        Qt6::Core
)
target_compile_definitions(
    test_better_pull_review_github_contract_tests
    PRIVATE
        BPR_GITHUB_CONTRACT_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures/github/contracts/v1"
)
bpr_enable_hardening(test_better_pull_review_github_contract_tests)
bpr_enable_warnings(test_better_pull_review_github_contract_tests)

bpr_add_network_isolated_test(
    NAME github_read_contract
    COMMAND test_better_pull_review_github_contract_tests
)

# RFC-BPR-0017's provider admission, asked without a network: the verdict table
# over every operation, the refusals that cost zero requests, and the door an
# eligible read goes through.
bpr_add_executable(
    test_better_pull_review_github_anonymous_read_contract_tests
    github_anonymous_read_contract_tests.cpp
)
target_link_libraries(
    test_better_pull_review_github_anonymous_read_contract_tests
    PRIVATE
        glaze::glaze
        BetterPullReview::AdapterGitHub
        BetterPullReview::Application
        Qt6::Core
        Qt6::Network
        Threads::Threads
)
target_include_directories(
    test_better_pull_review_github_anonymous_read_contract_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
# The promoted captures and the review packet they were promoted from. The
# test binds one to the other by digest, so a promoted body that drifts from
# the recorded capture fails rather than becoming its own authority.
target_compile_definitions(
    test_better_pull_review_github_anonymous_read_contract_tests
    PRIVATE
        BPR_RFC0017_ANONYMOUS_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures/github/rfc0017-anonymous-v001"
        BPR_RFC0017_ANONYMOUS_CAPTURE_ROOT="${PROJECT_SOURCE_DIR}/docs/verification/reviews/rfc0017-anonymous-probe-v001"
)
bpr_enable_hardening(
    test_better_pull_review_github_anonymous_read_contract_tests
)
bpr_enable_warnings(
    test_better_pull_review_github_anonymous_read_contract_tests
)

bpr_add_network_isolated_test(
    NAME github_anonymous_read_contract
    COMMAND test_better_pull_review_github_anonymous_read_contract_tests
)

bpr_add_executable(
    test_better_pull_review_github_device_authorization_contract_tests
    github_device_authorization_contract_tests.cpp
)
target_link_libraries(
    test_better_pull_review_github_device_authorization_contract_tests
    PRIVATE
        glaze::glaze
        BetterPullReview::AdapterGitHub
        Qt6::Core
)
target_include_directories(
    test_better_pull_review_github_device_authorization_contract_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    test_better_pull_review_github_device_authorization_contract_tests
    PRIVATE
        BPR_GITHUB_DEVICE_AUTHORIZATION_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures/github/device-authorization/v1"
        # A separate root, deliberately. The recorded tree above is digest-bound
        # to an owner-accepted attended sitting that captured no renewal, so the
        # documented-not-recorded refresh shapes cannot be filed inside it.
        BPR_GITHUB_DEVICE_DOCUMENTED_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures/github/device-authorization/documented-v1"
)
bpr_enable_hardening(test_better_pull_review_github_device_authorization_contract_tests)
bpr_enable_warnings(test_better_pull_review_github_device_authorization_contract_tests)

bpr_add_network_isolated_test(
    NAME github_device_authorization_contract
    COMMAND test_better_pull_review_github_device_authorization_contract_tests
)

bpr_add_executable(
    test_better_pull_review_github_mutation_contract_tests
    github_mutation_contract_tests.cpp
)
target_link_libraries(
    test_better_pull_review_github_mutation_contract_tests
    PRIVATE
        glaze::glaze
        BetterPullReview::AdapterGitHub
        BetterPullReview::Application
        Qt6::Core
)
target_include_directories(
    test_better_pull_review_github_mutation_contract_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    test_better_pull_review_github_mutation_contract_tests
    PRIVATE
        "BPR_REACTIONS_FIXTURE_DIR=\"${PROJECT_SOURCE_DIR}/tests/fixtures/github/reactions-v1\""
)
bpr_enable_hardening(test_better_pull_review_github_mutation_contract_tests)
bpr_enable_warnings(test_better_pull_review_github_mutation_contract_tests)
bpr_add_network_isolated_test(
    NAME github_mutation_contract
    COMMAND test_better_pull_review_github_mutation_contract_tests
)

# [[BPR-1175]]. The publication catalog's exact REST contract and account-bound session.
bpr_add_executable(test_better_pull_review_github_publication_contract_tests github_publication_contract_tests.cpp)
target_link_libraries(test_better_pull_review_github_publication_contract_tests PRIVATE glaze::glaze BetterPullReview::AdapterGitHub BetterPullReview::Application Qt6::Core)
target_include_directories(test_better_pull_review_github_publication_contract_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src")
bpr_enable_hardening(test_better_pull_review_github_publication_contract_tests)
bpr_enable_warnings(test_better_pull_review_github_publication_contract_tests)
bpr_add_network_isolated_test(NAME github_publication_contract COMMAND test_better_pull_review_github_publication_contract_tests)

bpr_add_executable(
    test_better_pull_review_live_review_mutation_provider_tests
    live_review_mutation_provider_tests.cpp
)
	target_link_libraries(
	    test_better_pull_review_live_review_mutation_provider_tests
	    PRIVATE
	        BetterPullReview::AdapterGitHub
	        BetterPullReview::AdapterSQLite
	        BetterPullReview::Application
        Qt6::Core
)
target_include_directories(
    test_better_pull_review_live_review_mutation_provider_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
bpr_enable_hardening(test_better_pull_review_live_review_mutation_provider_tests)
bpr_enable_warnings(test_better_pull_review_live_review_mutation_provider_tests)
bpr_add_network_isolated_test(
    NAME live_review_mutation_provider
    COMMAND test_better_pull_review_live_review_mutation_provider_tests
)
bpr_add_network_isolated_test(
    NAME live_review_mutation_restart_recovery
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/review_mutation_restart_recovery_tests.py"
        --executable
        "$<TARGET_FILE:test_better_pull_review_live_review_mutation_provider_tests>"
)

bpr_add_executable(
    test_better_pull_review_live_review_submission_provider_tests
    live_review_submission_provider_tests.cpp
)
target_link_libraries(
    test_better_pull_review_live_review_submission_provider_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        BetterPullReview::AdapterSQLite
        BetterPullReview::Application
        Qt6::Core
)
target_include_directories(
    test_better_pull_review_live_review_submission_provider_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
bpr_enable_hardening(test_better_pull_review_live_review_submission_provider_tests)
bpr_enable_warnings(test_better_pull_review_live_review_submission_provider_tests)
bpr_add_network_isolated_test(
    NAME live_review_submission_provider
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_live_review_submission_provider_tests
)
bpr_add_network_isolated_test(
    NAME live_review_submission_restart_recovery
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/review_submission_restart_recovery_tests.py"
        --executable
        "$<TARGET_FILE:test_better_pull_review_live_review_submission_provider_tests>"
)

bpr_add_executable(
    test_better_pull_review_github_paged_read_tests
    github_paged_read_tests.cpp
)
target_link_libraries(
    test_better_pull_review_github_paged_read_tests
    PRIVATE
        glaze::glaze
        BetterPullReview::AdapterGitHub
        BetterPullReview::Application
        Qt6::Core
)
target_include_directories(
    test_better_pull_review_github_paged_read_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    test_better_pull_review_github_paged_read_tests
    PRIVATE
        BPR_RFC0017_ANONYMOUS_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures/github/rfc0017-anonymous-v001"
)
bpr_enable_hardening(
    test_better_pull_review_github_paged_read_tests
)
bpr_enable_warnings(
    test_better_pull_review_github_paged_read_tests
)

bpr_add_network_isolated_test(
    NAME github_paged_read
    COMMAND test_better_pull_review_github_paged_read_tests
)

bpr_add_executable(
    test_better_pull_review_github_provider_tests
    github_provider_tests.cpp
)
target_link_libraries(
    test_better_pull_review_github_provider_tests
    PRIVATE
        glaze::glaze
        BetterPullReview::AdapterGitHub
        BetterPullReview::Application
        Qt6::Core
        Qt6::Network
        Threads::Threads
)
target_include_directories(
    test_better_pull_review_github_provider_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    test_better_pull_review_github_provider_tests
    PRIVATE
        BPR_GITHUB_CAPABILITY_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures/github/capabilities/v1"
)
bpr_enable_hardening(
    test_better_pull_review_github_provider_tests
)
bpr_enable_warnings(
    test_better_pull_review_github_provider_tests
)

bpr_add_network_isolated_test(
    NAME github_provider
    COMMAND test_better_pull_review_github_provider_tests
)
set_tests_properties(
    github_provider
    PROPERTIES
        TIMEOUT 30
)

bpr_add_executable(
    test_better_pull_review_github_host_profile_tests
    github_host_profile_tests.cpp
)
target_link_libraries(
    test_better_pull_review_github_host_profile_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
)
target_include_directories(
    test_better_pull_review_github_host_profile_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
bpr_enable_hardening(test_better_pull_review_github_host_profile_tests)
bpr_enable_warnings(test_better_pull_review_github_host_profile_tests)

bpr_add_network_isolated_test(
    NAME github_host_profile
    COMMAND test_better_pull_review_github_host_profile_tests
)

bpr_add_executable(
    test_better_pull_review_github_capability_probe_tests
    github_capability_probe_tests.cpp
)
target_link_libraries(
    test_better_pull_review_github_capability_probe_tests
    PRIVATE
        glaze::glaze
        BetterPullReview::AdapterGitHub
        Qt6::Core
)
target_include_directories(
    test_better_pull_review_github_capability_probe_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    test_better_pull_review_github_capability_probe_tests
    PRIVATE
        BPR_GITHUB_CAPABILITY_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures/github/capabilities/v1"
)
bpr_enable_hardening(
    test_better_pull_review_github_capability_probe_tests
)
bpr_enable_warnings(
    test_better_pull_review_github_capability_probe_tests
)

bpr_add_network_isolated_test(
    NAME github_capability_probe
    COMMAND test_better_pull_review_github_capability_probe_tests
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

    bpr_add_executable(
        test_better_pull_review_macos_keychain_contract_tests
        macos_keychain_contract_tests.cpp
    )
    target_link_libraries(
        test_better_pull_review_macos_keychain_contract_tests
        PRIVATE
            BetterPullReview::AdapterGitHub
            BetterPullReview::Application
            "${BPR_TEST_MACOS_SECURITY_FRAMEWORK}"
            "${BPR_TEST_MACOS_COREFOUNDATION_FRAMEWORK}"
    )
    target_include_directories(
        test_better_pull_review_macos_keychain_contract_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
    )
    bpr_enable_hardening(
        test_better_pull_review_macos_keychain_contract_tests
    )
    bpr_enable_warnings(
        test_better_pull_review_macos_keychain_contract_tests
    )

    bpr_add_network_isolated_test(
        NAME macos_keychain_contract
        COMMAND test_better_pull_review_macos_keychain_contract_tests
    )

    bpr_add_executable(
        test_better_pull_review_macos_keychain_native_tests
        macos_keychain_native_tests.cpp
    )
    target_link_libraries(
        test_better_pull_review_macos_keychain_native_tests
        PRIVATE
            BetterPullReview::AdapterGitHub
            BetterPullReview::Application
            "${BPR_TEST_MACOS_SECURITY_FRAMEWORK}"
            "${BPR_TEST_MACOS_COREFOUNDATION_FRAMEWORK}"
    )
    target_include_directories(
        test_better_pull_review_macos_keychain_native_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
    )
    bpr_enable_hardening(
        test_better_pull_review_macos_keychain_native_tests
    )
    bpr_enable_warnings(
        test_better_pull_review_macos_keychain_native_tests
    )

    bpr_add_network_isolated_test(
        NAME macos_keychain_native_setup
        COMMAND
            test_better_pull_review_macos_keychain_native_tests
            --fixture-setup
    )
    bpr_add_network_isolated_test(
        NAME macos_keychain_native
        COMMAND test_better_pull_review_macos_keychain_native_tests
    )
    bpr_add_network_isolated_test(
        NAME macos_keychain_native_cleanup
        COMMAND
            test_better_pull_review_macos_keychain_native_tests
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
    bpr_add_executable(
        test_better_pull_review_linux_secret_service_contract_tests
        linux_secret_service_contract_tests.cpp
    )
    target_link_libraries(
        test_better_pull_review_linux_secret_service_contract_tests
        PRIVATE
            BetterPullReview::AdapterGitHub
            BetterPullReview::Application
            LibSecret::LibSecret
            Threads::Threads
    )
    target_include_directories(
        test_better_pull_review_linux_secret_service_contract_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
    )
    bpr_enable_hardening(
        test_better_pull_review_linux_secret_service_contract_tests
    )
    bpr_enable_warnings(
        test_better_pull_review_linux_secret_service_contract_tests
    )

    bpr_add_network_isolated_test(
        NAME linux_secret_service_contract
        COMMAND
            test_better_pull_review_linux_secret_service_contract_tests
    )

    bpr_add_executable(
        test_better_pull_review_linux_secret_service_native_tests
        linux_secret_service_native_tests.cpp
    )
    target_link_libraries(
        test_better_pull_review_linux_secret_service_native_tests
        PRIVATE
            BetterPullReview::AdapterGitHub
            BetterPullReview::Application
            LibSecret::LibSecret
            Threads::Threads
    )
    target_include_directories(
        test_better_pull_review_linux_secret_service_native_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
    )
    bpr_enable_hardening(
        test_better_pull_review_linux_secret_service_native_tests
    )
    bpr_enable_warnings(
        test_better_pull_review_linux_secret_service_native_tests
    )

    # [[BPR-1117]]. The native rows write, replace and delete an item in the
    # collection the `default` alias names, so they refuse every session bus
    # that is not designated for tests. Each runs through the harness, which
    # starts a private dbus-daemon and a test-only gnome-keyring, designates
    # that bus, and stops both by exact PID when the row ends. No row reaches a
    # bus the harness did not start, and none needs a desktop session.
    set(
        bpr_linux_secret_service_private_session
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tests/scripts/linux_secret_service_private_session.py"
    )
    set(
        bpr_linux_secret_service_native_executable
        "$<TARGET_FILE:test_better_pull_review_linux_secret_service_native_tests>"
    )
    bpr_add_network_isolated_test(
        NAME linux_secret_service_native_setup
        COMMAND
            ${bpr_linux_secret_service_private_session}
            --
            ${bpr_linux_secret_service_native_executable}
            --fixture-setup
    )
    bpr_add_network_isolated_test(
        NAME linux_secret_service_native
        COMMAND
            ${bpr_linux_secret_service_private_session}
            --
            ${bpr_linux_secret_service_native_executable}
    )
    bpr_add_network_isolated_test(
        NAME linux_secret_service_native_cleanup
        COMMAND
            ${bpr_linux_secret_service_private_session}
            --
            ${bpr_linux_secret_service_native_executable}
            --fixture-cleanup
    )
    # [[BPR-1117]]. Pins the refusals: a missing, malformed or stale
    # designation, and a bus other than the designated one by address or by
    # runtime directory, each end the executable with its named line before
    # any vault call. The control's buses carry no keyring.
    bpr_add_network_isolated_test(
        NAME linux_secret_service_refusal_control
        COMMAND
            ${bpr_linux_secret_service_private_session}
            --refusal-control
            --
            ${bpr_linux_secret_service_native_executable}
    )
    set_tests_properties(
        linux_secret_service_refusal_control
        PROPERTIES
            TIMEOUT 60
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
    bpr_add_executable(
        test_better_pull_review_windows_credential_manager_contract_tests
        windows_credential_manager_contract_tests.cpp
    )
    target_link_libraries(
        test_better_pull_review_windows_credential_manager_contract_tests
        PRIVATE
            BetterPullReview::AdapterGitHub
            BetterPullReview::Application
            Advapi32
    )
    target_include_directories(
        test_better_pull_review_windows_credential_manager_contract_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
    )
    bpr_enable_hardening(
        test_better_pull_review_windows_credential_manager_contract_tests
    )
    bpr_enable_warnings(
        test_better_pull_review_windows_credential_manager_contract_tests
    )

    bpr_add_network_isolated_test(
        NAME windows_credential_manager_contract
        COMMAND
            test_better_pull_review_windows_credential_manager_contract_tests
    )

    bpr_add_executable(
        test_better_pull_review_windows_credential_manager_native_tests
        windows_credential_manager_native_tests.cpp
    )
    target_link_libraries(
        test_better_pull_review_windows_credential_manager_native_tests
        PRIVATE
            BetterPullReview::AdapterGitHub
            BetterPullReview::Application
            Advapi32
            Bcrypt
    )
    target_include_directories(
        test_better_pull_review_windows_credential_manager_native_tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
    )
    bpr_enable_hardening(
        test_better_pull_review_windows_credential_manager_native_tests
    )
    bpr_enable_warnings(
        test_better_pull_review_windows_credential_manager_native_tests
    )

    bpr_add_network_isolated_test(
        NAME windows_credential_manager_native_setup
        WINDOWS_UNCONTAINED
        COMMAND
            test_better_pull_review_windows_credential_manager_native_tests
            --fixture-setup
    )
    bpr_add_network_isolated_test(
        NAME windows_credential_manager_native
        WINDOWS_UNCONTAINED
        COMMAND
            test_better_pull_review_windows_credential_manager_native_tests
    )
    bpr_add_network_isolated_test(
        NAME windows_credential_manager_native_cleanup
        WINDOWS_UNCONTAINED
        COMMAND
            test_better_pull_review_windows_credential_manager_native_tests
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

if(APPLE)
    # Explicit RFC-0078 native qualification, requiring stock gh and deliberate
    # synthetic Keychain or designated BPR-Test input. Never a routine CTest.
    bpr_add_executable(test_better_pull_review_strict_cli_native_probe strict_cli_native_probe.cpp)
    target_link_libraries(test_better_pull_review_strict_cli_native_probe PRIVATE BetterPullReview::AdapterGitHub BetterPullReview::Application Qt6::Core)
    target_include_directories(test_better_pull_review_strict_cli_native_probe PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src")
    bpr_enable_hardening(test_better_pull_review_strict_cli_native_probe)
    bpr_enable_warnings(test_better_pull_review_strict_cli_native_probe)
    bpr_add_executable(test_better_pull_review_gh_cli_git_smart_transport_tests gh_cli_git_smart_transport_tests.cpp)
    target_link_libraries(test_better_pull_review_gh_cli_git_smart_transport_tests PRIVATE BetterPullReview::AdapterGitHub BetterPullReview::Application Qt6::Core)
    target_include_directories(test_better_pull_review_gh_cli_git_smart_transport_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src")
    bpr_enable_hardening(test_better_pull_review_gh_cli_git_smart_transport_tests)
    bpr_enable_warnings(test_better_pull_review_gh_cli_git_smart_transport_tests)
    # macOS refuses nested sandbox initialization. This closed self fixture
    # has no network implementation and local Git admits no protocols; gh's
    # production sandbox remains active inside the test.
    #
    # BPR-0802. So this row cannot go through bpr_add_network_isolated_test,
    # whose macOS command is the sandbox runner. It is instead the one named
    # exception in scripts/verify_test_network_isolation.py, which pins this
    # exact raw add_test, requires the executable to run with no wrapper or
    # argument, and inspects the built executable on every run for any
    # bpr::app:: symbol: a row outside the sandbox must not be able to compose
    # the application, its browser opener or its GitHub CLI factory.
    #
    # What the seam does apply here, it applies, from the seam's own function:
    # the boundary marker and the pre-boundary scrubs, which the gate checks on
    # this row as on every other. Not the bpr_network_isolated label, which
    # would claim a sandbox this row does not run in, and not the runner
    # prefix, which is the nesting itself.
    #
    # BPR-1496. The course launches about 970 sandboxed fake gh processes
    # (about 35 ms each; 84 exact credential acquisitions of six launches
    # each since BPR-1425/1426), which is genuine work: about 35 s on a quiet
    # Mac (macOS, 2026-10-13, load 13 to 21). Its two ten-second probe
    # deadlines run beside it on their own root. Before they did, the course
    # took 95 s at load 97 and timed out at 60 s in the full gate; 90 s leaves
    # room for a loaded gate.
    add_test(NAME gh_cli_git_smart_transport COMMAND test_better_pull_review_gh_cli_git_smart_transport_tests)
    set_tests_properties(gh_cli_git_smart_transport PROPERTIES TIMEOUT 90 LABELS bpr_local_only_fixture)
    bpr_apply_automated_test_environment(gh_cli_git_smart_transport)
endif()

bpr_add_network_isolated_test(NAME github_capability_content_policy COMMAND test_better_pull_review_github_capability_probe_tests --audit-content-policy)
bpr_add_network_isolated_test(NAME github_provider_content_policy COMMAND test_better_pull_review_github_provider_tests --audit-content-policy)

# BPR-0564: pure bounded codec, using only fixture-minted endpoint provenance.
bpr_add_executable(
    test_better_pull_review_review_comment_permalink_tests
    review_comment_permalink_tests.cpp
)
target_link_libraries(
    test_better_pull_review_review_comment_permalink_tests
    PRIVATE glaze::glaze BetterPullReview::AdapterGitHub BetterPullReview::Application BetterPullReview::AdapterSQLite BetterPullReview::SQLite3
)
target_include_directories(
    test_better_pull_review_review_comment_permalink_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    test_better_pull_review_review_comment_permalink_tests
    PRIVATE BPR_APPLICATION_TESTING=1
)
bpr_enable_hardening(test_better_pull_review_review_comment_permalink_tests)
if(WIN32)
    # [[BPR-1126]]. scratch_root.hpp makes a Windows root with the platform
    # target's owner-private descriptor.
    target_link_libraries(test_better_pull_review_review_comment_permalink_tests PRIVATE advapi32 BetterPullReview::Platform)
endif()
bpr_enable_warnings(test_better_pull_review_review_comment_permalink_tests)
bpr_add_network_isolated_test(
    NAME github_review_comment_permalink
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_review_comment_permalink_tests
)

bpr_add_executable(test_staged_review_session staged_review_session_tests.cpp)
target_link_libraries(test_staged_review_session PRIVATE BetterPullReview::AdapterGitHub BetterPullReview::AdapterSQLite BetterPullReview::Application Qt6::Core)
target_include_directories(test_staged_review_session PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src")
bpr_enable_hardening(test_staged_review_session)
bpr_enable_warnings(test_staged_review_session)
bpr_add_network_isolated_test(NAME staged_review_session WINDOWS_UNCONTAINED COMMAND test_staged_review_session)
bpr_add_network_isolated_test(NAME staged_review_restart WINDOWS_UNCONTAINED COMMAND Python3::Interpreter "${PROJECT_SOURCE_DIR}/tests/scripts/staged_review_restart_tests.py" --executable "$<TARGET_FILE:test_staged_review_session>")

# RFC-BPR-0084. Body-image fetches over native HTTPS.
bpr_add_executable(
    test_better_pull_review_native_https_image_tests
    native_https_image_tests.cpp
)
target_link_libraries(
    test_better_pull_review_native_https_image_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        BetterPullReview::Application
        Qt6::Core
        Qt6::Network
        Threads::Threads
)
target_include_directories(
    test_better_pull_review_native_https_image_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
target_compile_definitions(
    test_better_pull_review_native_https_image_tests
    PRIVATE
        BPR_NATIVE_HTTPS_TESTING=1
)
bpr_enable_hardening(test_better_pull_review_native_https_image_tests)
bpr_enable_warnings(test_better_pull_review_native_https_image_tests)
bpr_add_network_isolated_test(
    NAME native_https_image_contract
    COMMAND test_better_pull_review_native_https_image_tests --contract
)
set_tests_properties(native_https_image_contract PROPERTIES TIMEOUT 60)
if(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(BPR_TEST_NATIVE_HTTPS_IMAGE_OPENSSL NAMES openssl REQUIRED)
    bpr_add_network_isolated_test(
        NAME native_https_image_fixture
        LOOPBACK_PORT_COUNT 1
        COMMAND
            Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tests/scripts/verify_native_https_images.py"
            --executable
            "$<TARGET_FILE:test_better_pull_review_native_https_image_tests>"
            --openssl
            "${BPR_TEST_NATIVE_HTTPS_IMAGE_OPENSSL}"
    )
    set_tests_properties(
        native_https_image_fixture
        PROPERTIES
            RESOURCE_LOCK native_https_loopback
            TIMEOUT 120
    )
endif()

# RFC-BPR-0084. The body-image resolution read.
bpr_add_executable(
    test_better_pull_review_markdown_image_resolution_tests
    markdown_image_resolution_tests.cpp
)
target_link_libraries(
    test_better_pull_review_markdown_image_resolution_tests
    PRIVATE
        BetterPullReview::AdapterGitHub
        BetterPullReview::Application
        Qt6::Core
)
target_include_directories(
    test_better_pull_review_markdown_image_resolution_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
bpr_enable_hardening(test_better_pull_review_markdown_image_resolution_tests)
bpr_enable_warnings(test_better_pull_review_markdown_image_resolution_tests)
bpr_add_network_isolated_test(
    NAME github_markdown_image_resolution
    COMMAND test_better_pull_review_markdown_image_resolution_tests
)

# [[BPR-1741]]. The live reaction and resolution course against the
# one owner-designated disposable repository. Not a test row and not built by
# default: it reaches GitHub.com through gh, so only an explicit configure
# builds it, and tests/scripts/reaction_live_course.py runs it by hand.
option(BPR_BUILD_LIVE_REACTION_COURSE "Build the live reaction course (reaches github.com/MikeMarcin/BPR-Test through gh)" OFF)
if(BPR_BUILD_LIVE_REACTION_COURSE)
    bpr_add_executable(tool_reaction_live_course reaction_live_course.cpp)
    target_link_libraries(
        tool_reaction_live_course
        PRIVATE
            BetterPullReview::AdapterGitHub
            BetterPullReview::Application
            Qt6::Core
    )
    target_include_directories(tool_reaction_live_course PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src")
    bpr_enable_hardening(tool_reaction_live_course)
    bpr_enable_warnings(tool_reaction_live_course)
endif()

# [[BPR-1745]]. The live outdated-thread reply course's process: never a
# CTest row, and not built by default. Configure with
# -DBPR_BUILD_LIVE_OUTDATED_REPLY_COURSE=ON; tests/scripts/outdated_reply_live_course.py
# runs it by hand against MikeMarcin/BPR-Test.
option(BPR_BUILD_LIVE_OUTDATED_REPLY_COURSE "Build the live outdated-thread reply course (reaches github.com/MikeMarcin/BPR-Test through gh)" OFF)
if(BPR_BUILD_LIVE_OUTDATED_REPLY_COURSE)
    bpr_add_executable(tool_outdated_reply_live_course outdated_reply_live_course.cpp)
    target_link_libraries(
        tool_outdated_reply_live_course
        PRIVATE
            BetterPullReview::AdapterGitHub
            BetterPullReview::AdapterSQLite
            BetterPullReview::Application
            Qt6::Core
    )
    target_include_directories(tool_outdated_reply_live_course PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src")
    target_compile_definitions(tool_outdated_reply_live_course PRIVATE BPR_APPLICATION_TESTING=1)
    bpr_enable_hardening(tool_outdated_reply_live_course)
    bpr_enable_warnings(tool_outdated_reply_live_course)
endif()

# [[BPR-PENDING-AX]]. The live course of thread replies on the GitHub CLI
# connection: the app's own gh path (gh_cli_transport over the shipped
# process runner) against MikeMarcin/BPR-Test. Never a CTest row, and not built
# by default. Configure with -DBPR_BUILD_LIVE_CLI_REPLY_COURSE=ON;
# tests/scripts/cli_reply_live_course.py runs it by hand.
option(BPR_BUILD_LIVE_CLI_REPLY_COURSE "Build the live CLI-connection reply course (reaches github.com/MikeMarcin/BPR-Test through gh)" OFF)
if(BPR_BUILD_LIVE_CLI_REPLY_COURSE)
    bpr_add_executable(tool_cli_reply_live_course cli_reply_live_course.cpp)
    target_link_libraries(
        tool_cli_reply_live_course
        PRIVATE
            BetterPullReview::AdapterGitHub
            BetterPullReview::AdapterSQLite
            BetterPullReview::Application
            Qt6::Core
    )
    target_include_directories(tool_cli_reply_live_course PRIVATE "${PROJECT_SOURCE_DIR}/src/adapters/github/src")
    target_compile_definitions(tool_cli_reply_live_course PRIVATE BPR_APPLICATION_TESTING=1)
    bpr_enable_hardening(tool_cli_reply_live_course)
    bpr_enable_warnings(tool_cli_reply_live_course)
endif()

# RFC-BPR-0084. The live image course's process. CTest runs only its offline
# request compilation; tests/scripts/markdown_image_live_course.py drives it
# against MikeMarcin/BPR-Test by hand.
bpr_add_executable(test_better_pull_review_markdown_image_live_course markdown_image_live_course.cpp)
target_link_libraries(
    test_better_pull_review_markdown_image_live_course
    PRIVATE
        BetterPullReview::AdapterGitHub
        BetterPullReview::AdapterQt
        BetterPullReview::Application
        Qt6::Core
        Qt6::Network
)
target_include_directories(
    test_better_pull_review_markdown_image_live_course
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/adapters/github/src"
)
bpr_enable_hardening(test_better_pull_review_markdown_image_live_course)
bpr_enable_warnings(test_better_pull_review_markdown_image_live_course)
bpr_add_network_isolated_test(
    NAME markdown_image_live_course_request
    COMMAND test_better_pull_review_markdown_image_live_course --resolution-body MikeMarcin BPR-Test 73 PR_kwDOTyvof88AAAABIypVGw
)
set_tests_properties(
    markdown_image_live_course_request
    PROPERTIES
        PASS_REGULAR_EXPRESSION "query ResolveBodyImages"
        TIMEOUT 30
)
