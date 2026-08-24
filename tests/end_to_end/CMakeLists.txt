if(APPLE OR WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(bpr_native_platform_vault_self_test_result "true")
else()
    set(bpr_native_platform_vault_self_test_result "false")
endif()

# An automated application run bounds itself in wall clock so that it always
# reports its stage, verdict, and evidence before CTest kills the process. A
# run killed by the harness reports nothing at all: that is what made a stalled
# stage unattributable, and it is the whole reason these two numbers exist.
#
# They only mean anything together, so this states the relationship once and
# refuses a pair that does not hold. `RUNS` is how many times the test starts
# the application under a budget. `RESERVE_MS` is everything the budget does
# not cover: process start, shutdown, writing evidence, and any invocation that
# exits during argument parsing before a run is ever scheduled.
#
# The rule is that the worst case the application permits itself must fit
# inside three quarters of the harness allowance. The remaining quarter belongs
# to CTest and is never meant to be spent.
#
# Setting `TIMEOUT` only through this function is deliberate. A timeout raised
# or a budget lowered on its own is exactly the drift that produced the
# original failure, and here it cannot be done without restating both.
function(bpr_bound_automated_run bpr_test_name)
    cmake_parse_arguments(
        PARSE_ARGV
        1
        arg
        ""
        "TIMEOUT;BUDGET_MS;RUNS;RESERVE_MS"
        ""
    )
    foreach(required IN ITEMS TIMEOUT BUDGET_MS RUNS RESERVE_MS)
        if(NOT DEFINED arg_${required})
            message(
                FATAL_ERROR
                "${bpr_test_name}: ${required} is required to bound the run"
            )
        endif()
    endforeach()

    math(
        EXPR
        bpr_worst_case_ms
        "${arg_RUNS} * ${arg_BUDGET_MS} + ${arg_RESERVE_MS}"
    )
    math(EXPR bpr_harness_ms "${arg_TIMEOUT} * 1000")
    math(EXPR bpr_permitted_ms "${bpr_harness_ms} * 3 / 4")
    if(bpr_worst_case_ms GREATER bpr_permitted_ms)
        message(
            FATAL_ERROR
            "${bpr_test_name}: the application would permit itself "
            "${bpr_worst_case_ms} ms (${arg_RUNS} run(s) of ${arg_BUDGET_MS} ms "
            "plus ${arg_RESERVE_MS} ms outside the budget), which exceeds the "
            "${bpr_permitted_ms} ms three-quarter share of its ${arg_TIMEOUT} s "
            "CTest timeout. A stalled stage would be killed by the harness with "
            "no stage, verdict, or evidence reported."
        )
    endif()

    math(EXPR bpr_headroom_ms "${bpr_harness_ms} - ${bpr_worst_case_ms}")
    message(
        STATUS
        "bpr: ${bpr_test_name} bounds itself at ${bpr_worst_case_ms} ms inside "
        "a ${bpr_harness_ms} ms timeout (${bpr_headroom_ms} ms headroom)"
    )
    set_tests_properties("${bpr_test_name}" PROPERTIES TIMEOUT ${arg_TIMEOUT})
endfunction()

# Budgets, each read together with the timeout it sits inside. See
# `bpr_bound_automated_run` above and `default_automated_run_budget` in
# `src/app/main.cpp`.
#
# Each is set against what the run actually takes on an idle host, with room
# for a loaded one. Measured on macOS 15.7.7 arm64, Qt 6.11.1:
#
#   self test                       0.5 s   budget 8 s     16x
#   fixture render and workbench   12-13 s  budget 40 s     3x
#   native appearance probe        ~11 s    budget 30 s   2.7x
#   epoch phase                     25 s    budget 60 s   2.4x
#
# A budget is not a target. It is the point past which waiting has stopped
# being useful and the run should say what it was waiting for.
set(bpr_self_test_budget_ms 8000)
set(bpr_fixture_run_budget_ms 40000)
set(bpr_native_appearance_budget_ms 30000)
set(bpr_epoch_phase_budget_ms 60000)
set(bpr_stall_budget_ms 5000)

set(bpr_test_qt_root "")
if(DEFINED Qt6_ROOT)
    set(bpr_test_qt_root "${Qt6_ROOT}")
endif()
set(bpr_test_qt_dir "")
if(DEFINED BPR_PRODUCER_QT6_DIR)
    set(bpr_test_qt_dir "${BPR_PRODUCER_QT6_DIR}")
endif()

bpr_add_network_isolated_test(
    NAME application_public_path
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_public_path
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_UNAVAILABLE_OK[\r\n]+CLI_UNAVAILABLE_COUNT:1"
)
bpr_bound_automated_run(
    application_public_path
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

bpr_add_network_isolated_test(
    NAME application_public_path_ghes_cli_unavailable
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --self-test
        --ghes-host
        ghe.example.invalid
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_public_path_ghes_cli_unavailable
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+NATIVE_PLATFORM_VAULT_SELECTED:${bpr_native_platform_vault_self_test_result}[\r\n]+CLI_UNAVAILABLE_OK[\r\n]+CLI_UNAVAILABLE_COUNT:2"
)
bpr_bound_automated_run(
    application_public_path_ghes_cli_unavailable
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

bpr_add_executable(
    test_better_pull_review_worker_executor_tests
    worker_executor_tests.cpp
)

bpr_add_executable(
    test_better_pull_review_queue_composition_public_path
    queue_composition_public_path.cpp
)
target_link_libraries(
    test_better_pull_review_queue_composition_public_path
    PRIVATE
        BetterPullReview::Composition
)
target_compile_definitions(
    test_better_pull_review_queue_composition_public_path
    PRIVATE
        BPR_BUILD_TESTING=1
)
target_compile_features(
    test_better_pull_review_queue_composition_public_path
    PRIVATE
        cxx_std_23
)
bpr_enable_hardening(test_better_pull_review_queue_composition_public_path)
bpr_enable_warnings(test_better_pull_review_queue_composition_public_path)

target_link_libraries(
    test_better_pull_review_worker_executor_tests
    PRIVATE
        BetterPullReview::Composition
)
target_compile_features(
    test_better_pull_review_worker_executor_tests
    PRIVATE
        cxx_std_23
)
bpr_enable_hardening(test_better_pull_review_worker_executor_tests)
bpr_enable_warnings(test_better_pull_review_worker_executor_tests)

bpr_add_network_isolated_test(
    NAME application_worker_executor
    COMMAND test_better_pull_review_worker_executor_tests
)
set_tests_properties(
    application_worker_executor
    PROPERTIES
        PASS_REGULAR_EXPRESSION "worker-executor-tests:pass"
        TIMEOUT 15
)

# BPR-0381. What a timed-out public-path wait says, asserted directly rather than
# only when something else is already broken. No Qt: the report formatting is
# standard C++ precisely so this can compile it without an application or an event
# loop, and the polling half that needs Qt is not under test here.
bpr_add_executable(
    test_better_pull_review_public_path_wait_tests
    public_path_wait_tests.cpp
)
target_compile_features(
    test_better_pull_review_public_path_wait_tests
    PRIVATE
        cxx_std_23
)
bpr_enable_hardening(test_better_pull_review_public_path_wait_tests)
bpr_enable_warnings(test_better_pull_review_public_path_wait_tests)

bpr_add_network_isolated_test(
    NAME application_public_path_wait
    COMMAND test_better_pull_review_public_path_wait_tests
)
set_tests_properties(
    application_public_path_wait
    PROPERTIES
        PASS_REGULAR_EXPRESSION "public-path-wait-tests:pass"
        TIMEOUT 15
)

set(bpr_application_composition_contract_command
    Python3::Interpreter
    "${CMAKE_CURRENT_SOURCE_DIR}/composition_contract_tests.py"
    --cmake
    "${CMAKE_COMMAND}"
    --cxx
    "${CMAKE_CXX_COMPILER}"
)
if(bpr_test_qt_root)
    list(
        APPEND
        bpr_application_composition_contract_command
        --qt-root
        "${bpr_test_qt_root}"
    )
endif()
if(bpr_test_qt_dir)
    list(
        APPEND
        bpr_application_composition_contract_command
        --qt-dir
        "${bpr_test_qt_dir}"
    )
endif()
list(
    APPEND
    bpr_application_composition_contract_command
    --cpm-source-cache
    "${CPM_SOURCE_CACHE}"
)
bpr_add_network_isolated_test(
    NAME application_composition_contract
    WINDOWS_UNCONTAINED
    COMMAND ${bpr_application_composition_contract_command}
)
# No PASS_REGULAR_EXPRESSION. This is a `unittest` suite and its exit code is
# already the verdict; setting the property makes CTest ignore that exit code
# and pass on a substring match instead. "OK" is satisfied by any incidental
# `..._OK` marker in captured output, which is how a failing suite reported
# Passed.
set_tests_properties(
    application_composition_contract
    PROPERTIES
        TIMEOUT 600
)

set(bpr_public_path_fixture_environment "QT_QPA_PLATFORM=offscreen")
if(UNIX AND NOT APPLE)
    list(APPEND
        bpr_public_path_fixture_environment
        "QT_STYLE_OVERRIDE=Windows"
    )
endif()

bpr_add_executable(
    test_better_pull_review_git_diagnostic_wrapper
    "${PROJECT_SOURCE_DIR}/tests/fixtures/git/git_diagnostic_wrapper_main.cpp"
)
target_compile_features(
    test_better_pull_review_git_diagnostic_wrapper
    PRIVATE
        cxx_std_23
)
target_compile_definitions(
    test_better_pull_review_git_diagnostic_wrapper
    PRIVATE
        BPR_REAL_GIT_EXECUTABLE="$<TARGET_FILE:Git::Git>"
)
set_target_properties(
    test_better_pull_review_git_diagnostic_wrapper
    PROPERTIES
        OUTPUT_NAME git
        RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_CURRENT_BINARY_DIR}/git-diagnostic-wrapper"
)
bpr_enable_hardening(test_better_pull_review_git_diagnostic_wrapper)
bpr_enable_warnings(test_better_pull_review_git_diagnostic_wrapper)

bpr_add_executable(
    test_better_pull_review_agent_invocation_composition_public_path
    agent_invocation_composition_public_path.cpp
)
target_link_libraries(
    test_better_pull_review_agent_invocation_composition_public_path
    PRIVATE
        BetterPullReview::Composition
)
target_compile_definitions(
    test_better_pull_review_agent_invocation_composition_public_path
    PRIVATE
        BPR_BUILD_TESTING=1
)
add_dependencies(
    test_better_pull_review_agent_invocation_composition_public_path
    test_better_pull_review_agent_canary
    test_better_pull_review_git_diagnostic_wrapper
)
bpr_enable_hardening(
    test_better_pull_review_agent_invocation_composition_public_path
)
bpr_enable_warnings(
    test_better_pull_review_agent_invocation_composition_public_path
)

foreach(
    course
    IN ITEMS
        positive
        construction-cut
        gesture-cut
        cancellation
        staleness
        cleanup-retry
        workspace-diagnostic
)
	if(course STREQUAL "positive")
		set(test_name application_agent_invocation_composition_contract)
	else()
        string(REPLACE "-" "_" course_name "${course}")
        set(test_name "application_agent_invocation_${course_name}_contract")
        set(pass_marker "agent-invocation-${course}:pass")
    endif()
	if(course STREQUAL "positive")
		bpr_add_network_isolated_test(
			NAME "${test_name}"
			WINDOWS_UNCONTAINED
			COMMAND
				"${CMAKE_COMMAND}"
				"-DBPR_APPLICATION=$<TARGET_FILE:test_better_pull_review_agent_invocation_composition_public_path>"
				"-DBPR_APPLICATION_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/agent_invocation_composition_public_path.cpp"
				"-DBPR_CANARY=$<TARGET_FILE:test_better_pull_review_agent_canary>"
				"-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/agent-invocation-composition-contract"
				-P
				"${CMAKE_CURRENT_SOURCE_DIR}/application_agent_invocation_composition_contract.cmake"
		)
	else()
		if(course STREQUAL "workspace-diagnostic")
			set(
				course_extra_argument
				$<TARGET_FILE:test_better_pull_review_git_diagnostic_wrapper>
			)
		else()
			set(course_extra_argument "")
		endif()
		bpr_add_network_isolated_test(
			NAME "${test_name}"
			WINDOWS_UNCONTAINED
			COMMAND
				test_better_pull_review_agent_invocation_composition_public_path
				"${course}"
				$<TARGET_FILE:test_better_pull_review_agent_canary>
				${course_extra_argument}
		)
	endif()
    set_tests_properties(
        "${test_name}"
        PROPERTIES
            ENVIRONMENT "${bpr_public_path_fixture_environment};GITHUB_TOKEN=must-not-cross-agent-boundary"
            TIMEOUT 90
    )
	if(NOT course STREQUAL "positive")
		set_tests_properties(
			"${test_name}"
			PROPERTIES PASS_REGULAR_EXPRESSION "${pass_marker}"
		)
	endif()
endforeach()

bpr_add_network_isolated_test(
    NAME application_queue_composition_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:test_better_pull_review_queue_composition_public_path>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/queue-composition-contract"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_queue_composition_contract.cmake"
)
set_tests_properties(
    application_queue_composition_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "QUEUE_COMPOSITION_CONTRACT_OK"
)
bpr_bound_automated_run(
    application_queue_composition_contract
    TIMEOUT 75
    BUDGET_MS 25000
    RUNS 2
    RESERVE_MS 4000
)

bpr_add_network_isolated_test(
    NAME application_native_notification_attended_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/native-notification-attended-contract"
        "-DBPR_SOURCE=${CMAKE_SOURCE_DIR}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_native_notification_attended_contract.cmake"
)
if(APPLE)
    set_tests_properties(
        application_native_notification_attended_contract
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=cocoa"
    )
else()
    set_tests_properties(
        application_native_notification_attended_contract
        PROPERTIES ENVIRONMENT "${bpr_public_path_fixture_environment}"
    )
endif()
bpr_bound_automated_run(
    application_native_notification_attended_contract
    TIMEOUT 120
    BUDGET_MS 25000
    RUNS 3
    RESERVE_MS 4000
)

bpr_add_executable(
    test_better_pull_review_github_application_liveness_tests
    github_application_liveness_tests.cpp
)
target_link_libraries(
    test_better_pull_review_github_application_liveness_tests
    PRIVATE
        BetterPullReview::Composition
)
target_compile_features(
    test_better_pull_review_github_application_liveness_tests
    PRIVATE
        cxx_std_23
)
bpr_enable_hardening(test_better_pull_review_github_application_liveness_tests)
bpr_enable_warnings(test_better_pull_review_github_application_liveness_tests)

# Constructs the real application, so it needs the same offscreen platform the
# fixture runs use. Nothing is shown and no fixture is selected: the subject is
# the destructor, not a course.
bpr_add_network_isolated_test(
    NAME application_poller_liveness
    WINDOWS_UNCONTAINED
    COMMAND test_better_pull_review_github_application_liveness_tests
)
set_tests_properties(
    application_poller_liveness
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "github-application-liveness-tests:pass"
        TIMEOUT 60
)

bpr_add_network_isolated_test(
    NAME application_icon
    COMMAND
        app_better_pull_review
        --application-icon-probe
)
set_tests_properties(
    application_icon
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        PASS_REGULAR_EXPRESSION "APPLICATION_ICON_OK"
        TIMEOUT 15
)

bpr_add_network_isolated_test(
    NAME application_native_appearance_probe_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_BEFORE_RENDER=${CMAKE_CURRENT_BINARY_DIR}/native-appearance-before.png"
        "-DBPR_AFTER_RENDER=${CMAKE_CURRENT_BINARY_DIR}/native-appearance-after.png"
        "-DBPR_RUN_BUDGET_MS=${bpr_native_appearance_budget_ms}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/native_appearance_probe_contract.cmake"
)
set_tests_properties(
    application_native_appearance_probe_contract
    PROPERTIES
        PASS_REGULAR_EXPRESSION "NATIVE_APPEARANCE_PROBE_CONTRACT_OK"
)
# Two full-budget runs — the offscreen real-selection course and its cut wire.
# The reserve covers the 1 ms startup-budget course plus the eight
# configuration-failure invocations; each starts Qt but does not consume a full
# automated-run budget.
bpr_bound_automated_run(
    application_native_appearance_probe_contract
    TIMEOUT 100
    BUDGET_MS ${bpr_native_appearance_budget_ms}
    RUNS 2
    RESERVE_MS 12000
)

bpr_add_network_isolated_test(
    NAME application_github_public_path_github_com
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --github-fixture
        github-com
        --render-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/github-public-path-github-com.png"
        --automated-run-budget-ms
        ${bpr_fixture_run_budget_ms}
)
set_tests_properties(
    application_github_public_path_github_com
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+GITHUB_FIXTURE_OK:github.com[\r\n]+RENDER_OK"
)
bpr_bound_automated_run(
    application_github_public_path_github_com
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# The recorded public pull request, served through the same public path as the
# synthetic fixtures. A green run is the conjunction at main.cpp's evidence
# gate: all ten recorded interactions consumed in order, the capability probe
# answered once, one smart acquire/discover/ls-refs/fetch, and the pack
# imported through `git index-pack --strict
# --check-self-contained-and-connected` followed by `fsck --connectivity-only`.
bpr_add_network_isolated_test(
    NAME application_github_public_path_uiforetw_74
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --github-fixture
        uiforetw-74
        --render-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/github-public-path-uiforetw-74.png"
        --automated-run-budget-ms
        ${bpr_fixture_run_budget_ms}
)
set_tests_properties(
    application_github_public_path_uiforetw_74
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+GITHUB_FIXTURE_OK:uiforetw-74[\r\n]+RENDER_OK"
)
bpr_bound_automated_run(
    application_github_public_path_uiforetw_74
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

bpr_add_network_isolated_test(
    NAME application_uiforetw_selected_diff_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/uiforetw-selected-diff-contract"
        "-DBPR_RUN_BUDGET_MS=${bpr_fixture_run_budget_ms}"
        "-DBPR_SOURCE=${PROJECT_SOURCE_DIR}/src/app/src/github_application.cpp"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_uiforetw_selected_diff_contract.cmake"
)
set_tests_properties(
    application_uiforetw_selected_diff_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "UIFORETW_SELECTED_DIFF_CONTRACT_OK"
)
bpr_bound_automated_run(
    application_uiforetw_selected_diff_contract
    TIMEOUT 300
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 5
    RESERVE_MS 10000
)

bpr_add_network_isolated_test(
    NAME application_diff_workbench_public_path
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --github-fixture
        github-com
        --diff-workbench-probe
        --diff-interaction-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/bpr-0083-diff-interaction"
        --render-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/diff-workbench-public-path.png"
        --automated-run-budget-ms
        ${bpr_fixture_run_budget_ms}
)
set_tests_properties(
    application_diff_workbench_public_path
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION
            # One contiguous block, so every line the status dump prints between
            # the first and last marker has to be named here. An inserted line is
            # a mismatch rather than extra output, which is how BPR-0368's two
            # new markers left this test red from 9d63c4d until BPR-0393.
            #
            # SYNTAX_DISPLAYED is matched by shape on purpose. It reads 0:0 on
            # this course today, which is BPR-0390's defect; binding it to 1:1
            # here would take BPR-0390's own deterministic check away from it.
            "PUBLIC_PATH_OK[\r\n]+GITHUB_FIXTURE_OK:github.com[\r\n]+DIFF_WORKBENCH_OK[\r\n]+DIFF_INTERACTION_EVIDENCE_OK[\r\n]+SYNTAX_THEME_REPAINT_OK[\r\n]+RENDER_OK[\r\n]+REVIEW_LEDGER_PRESENT_ON_ENTRY:0[\r\n]+REVIEW_LEDGER_OK[\r\n]+WORKBENCH_HOST_COMPOSED_OK[\r\n]+APPEARANCE_HOST_COMPOSED_OK[\r\n]+SYNTAX_CLASSIFIER_COMPOSED_OK[\r\n]+SYNTAX_CLASSIFICATION:[1-9][0-9]*:[1-9][0-9]*[\r\n]+SYNTAX_DELIVERY:[0-9]+:[0-9]+:[0-9]+:[0-9]+:[0-9]+[\r\n]+SYNTAX_DISPLAYED:[01]:[01][\r\n]+SYNTAX_DISPLAYED_SPANS:[0-9]+:[0-9]+:[0-9]+:[01][\r\n]+APPEARANCE_SETTINGS:[a-z_]+:[0-9]+[\r\n]+NOTIFICATION_DELIVERY_COMPOSED_OK[\r\n]+NOTIFICATION_PERMISSION:[a-z-]+[\r\n]+NOTIFICATION_BUNDLE_IDENTITY:[A-Za-z0-9.-]+[\r\n]+APPEARANCE_DIVERGENCE:0[\r\n]+APPEARANCE_OBSERVED:[a-z]+:[a-z-]+:[0-9]+[\r\n]+APPEARANCE_APPLIED:#[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]:#[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]:from-profile"
)
bpr_bound_automated_run(
    application_diff_workbench_public_path
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

bpr_add_network_isolated_test(
    NAME application_workbench_composition_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
		"-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/workbench-composition-contract"
		"-DBPR_RUN_BUDGET_MS=${bpr_fixture_run_budget_ms}"
		"-DBPR_SOURCE=${PROJECT_SOURCE_DIR}/src/app/src/github_application.cpp"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_workbench_composition_contract.cmake"
)
set_tests_properties(
    application_workbench_composition_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "WORKBENCH_COMPOSITION_CONTRACT_OK"
)
bpr_bound_automated_run(
    application_workbench_composition_contract
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

bpr_add_network_isolated_test(
    NAME application_review_ledger_composition_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/review-ledger-composition-contract"
        "-DBPR_RUN_BUDGET_MS=${bpr_fixture_run_budget_ms}"
        "-DBPR_SOURCE=${PROJECT_SOURCE_DIR}/src/app/src/github_application.cpp"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_review_ledger_composition_contract.cmake"
)
set_tests_properties(
    application_review_ledger_composition_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "REVIEW_LEDGER_COMPOSITION_CONTRACT_OK"
)
bpr_bound_automated_run(
    application_review_ledger_composition_contract
    TIMEOUT 180
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 3
    RESERVE_MS 4000
)

bpr_add_network_isolated_test(
    NAME application_review_round_trip_composition_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_APP_SOURCE=${PROJECT_SOURCE_DIR}/src/app/src/github_application.cpp"
        "-DBPR_ISOLATION_SCRIPT=${PROJECT_SOURCE_DIR}/scripts/verify_live_mutation_isolation.py"
        "-DBPR_MANIFEST=${CMAKE_BINARY_DIR}/_bpr/$<CONFIG>/automated-executables.txt"
        "-DBPR_OBJECT_MANIFEST=${CMAKE_BINARY_DIR}/_bpr/$<CONFIG>/automated-executable-objects.txt"
        "-DBPR_PYTHON=$<TARGET_FILE:Python3::Interpreter>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/review-round-trip-composition-contract"
        "-DBPR_RUN_BUDGET_MS=${bpr_fixture_run_budget_ms}"
        "-DBPR_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_review_round_trip_composition_contract.cmake"
)
set_tests_properties(
    application_review_round_trip_composition_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
)
bpr_bound_automated_run(
    application_review_round_trip_composition_contract
    TIMEOUT 75
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 10000
)

bpr_add_network_isolated_test(
    NAME application_defect_composition_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/defect-composition-contract"
        "-DBPR_RUN_BUDGET_MS=${bpr_fixture_run_budget_ms}"
        "-DBPR_SOURCE=${PROJECT_SOURCE_DIR}/src/app/src/github_application.cpp"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_defect_composition_contract.cmake"
)
set_tests_properties(
    application_defect_composition_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "DEFECT_COMPOSITION_CONTRACT_OK"
)
bpr_bound_automated_run(
    application_defect_composition_contract
    TIMEOUT 120
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 2
    RESERVE_MS 4000
)

set(bpr_appearance_theme_budget_ms 50000)
bpr_add_network_isolated_test(
    NAME application_appearance_theme_customization_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/appearance-theme-customization-root"
        "-DBPR_THEME=${CMAKE_SOURCE_DIR}/tests/fixtures/appearance/contrast-safe-theme.bprtheme"
        "-DBPR_RUN_BUDGET_MS=${bpr_appearance_theme_budget_ms}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/application_appearance_theme_customization_contract.cmake"
)
set_tests_properties(
    application_appearance_theme_customization_contract
    PROPERTIES
        PASS_REGULAR_EXPRESSION "APPEARANCE_THEME_CUSTOMIZATION_CONTRACT_OK"
)
bpr_bound_automated_run(
    application_appearance_theme_customization_contract
    TIMEOUT 240
    BUDGET_MS ${bpr_appearance_theme_budget_ms}
    RUNS 3
    RESERVE_MS 10000
)

bpr_add_network_isolated_test(
    NAME application_fixture_verdict_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/fixture_verdict_contract.py"
        $<TARGET_FILE:app_better_pull_review>
)
set_tests_properties(
    application_fixture_verdict_contract
    PROPERTIES
        TIMEOUT 300
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "FIXTURE_VERDICT_OK"
)

bpr_add_network_isolated_test(
    NAME application_attended_launch_contract
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/attended_launch_contract.py"
        $<TARGET_FILE:app_better_pull_review>
)
set_tests_properties(
    application_attended_launch_contract
    PROPERTIES
        TIMEOUT 180
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "ATTENDED_LAUNCH_OK"
)

bpr_add_network_isolated_test(
    NAME application_diff_workbench_shutdown_public_path
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --github-fixture
        github-com
        --diff-workbench-shutdown-probe
        --automated-run-budget-ms
        ${bpr_fixture_run_budget_ms}
)
set_tests_properties(
    application_diff_workbench_shutdown_public_path
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+GITHUB_FIXTURE_OK:github.com[\r\n]+DIFF_WORKBENCH_SHUTDOWN_OK"
)
bpr_bound_automated_run(
    application_diff_workbench_shutdown_public_path
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

# The run's own bound, exercised. `--automated-stall-probe` holds the `open`
# stage forever, which is the shape of a hang: nothing crashes, nothing errors,
# a readiness condition simply never becomes true. What has to happen is that
# the application notices and reports the stage it was in, the verdict, and the
# evidence — rather than being killed with nothing to show.
#
# The 5 s budget inside a 30 s timeout is deliberately lopsided. An application
# that failed to bound itself would be killed by the harness at 30 s, print
# nothing, and fail this test; there is no arrangement of the numbers under
# which a missing bound still passes.
bpr_add_network_isolated_test(
    NAME application_automated_run_budget
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --github-fixture
        github-com
        --automated-stall-probe
        --automated-run-budget-ms
        ${bpr_stall_budget_ms}
)
set_tests_properties(
    application_automated_run_budget
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        # `PUBLIC_PATH_STAGE:2` is `open`, the third stage. It is asserted by
        # number because a number is what the evidence prints; if a stage is
        # ever inserted ahead of `open` this assertion is meant to be revisited
        # rather than to keep passing against a different stage.
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_STAGE:2[\r\n]+PUBLIC_PATH_VERDICT:[^\r\n]+[\r\n]+PUBLIC_PATH_GROUPS:[^\r\n]+[\r\n]+PUBLIC_PATH_BUDGET:expired=1 budget_ms=${bpr_stall_budget_ms} elapsed_ms=[0-9]+ stage_ticks=[0-9]+"
)
bpr_bound_automated_run(
    application_automated_run_budget
    TIMEOUT 30
    BUDGET_MS ${bpr_stall_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

bpr_add_network_isolated_test(
    NAME application_github_public_path_ghes_3_18
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --github-fixture
        ghes-3.18
        --render-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/github-public-path-ghes-3.18.png"
        --automated-run-budget-ms
        ${bpr_fixture_run_budget_ms}
)
set_tests_properties(
    application_github_public_path_ghes_3_18
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION
            "PUBLIC_PATH_OK[\r\n]+GITHUB_FIXTURE_OK:ghes-3.18[\r\n]+RENDER_OK"
)
bpr_bound_automated_run(
    application_github_public_path_ghes_3_18
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
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
    bpr_add_network_isolated_test(
        NAME application_epoch_persistence_${fixture_test_name}
        WINDOWS_UNCONTAINED
        COMMAND
            "${CMAKE_COMMAND}"
            "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
            "-DBPR_FIXTURE=${fixture_name}"
            "-DBPR_FIXTURE_LABEL=${fixture_label}"
            "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/epoch-persistence-${fixture_test_name}"
            "-DBPR_RUN_BUDGET_MS=${bpr_epoch_phase_budget_ms}"
            -P
            "${CMAKE_CURRENT_SOURCE_DIR}/epoch_persistence_contract.cmake"
    )
    set_tests_properties(
        application_epoch_persistence_${fixture_test_name}
        PROPERTIES
            ENVIRONMENT "${bpr_public_path_fixture_environment}"
            PASS_REGULAR_EXPRESSION
                "EPOCH_PERSISTENCE_CONTRACT_OK:${fixture_label}"
    )
    # Two budgeted runs: the seed phase and the reuse phase.
    bpr_bound_automated_run(
        application_epoch_persistence_${fixture_test_name}
        TIMEOUT 240
        BUDGET_MS ${bpr_epoch_phase_budget_ms}
        RUNS 2
        RESERVE_MS 4000
    )
endforeach()

bpr_add_network_isolated_test(
    NAME application_epoch_maintenance_public_path
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/epoch-maintenance-public-path"
        "-DBPR_RUN_BUDGET_MS=${bpr_epoch_phase_budget_ms}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/epoch_maintenance_public_path_contract.cmake"
)
set_tests_properties(
    application_epoch_maintenance_public_path
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "EPOCH_MAINTENANCE_PUBLIC_PATH_CONTRACT_OK"
)
# Four budgeted runs: seed, prune, verify-pruned-delete, verify-deleted.
bpr_bound_automated_run(
    application_epoch_maintenance_public_path
    TIMEOUT 360
    BUDGET_MS ${bpr_epoch_phase_budget_ms}
    RUNS 4
    RESERVE_MS 8000
)

bpr_add_network_isolated_test(
    NAME application_appearance_gesture_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/appearance-gesture-contract"
        "-DBPR_RUN_BUDGET_MS=${bpr_epoch_phase_budget_ms}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/appearance_gesture_contract.cmake"
)
set_tests_properties(
    application_appearance_gesture_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "APPEARANCE_GESTURE_CONTRACT_OK"
)
# Three budgeted runs: the fresh composition, the gesture, and the restart.
bpr_bound_automated_run(
    application_appearance_gesture_contract
    TIMEOUT 300
    BUDGET_MS ${bpr_epoch_phase_budget_ms}
    RUNS 3
    RESERVE_MS 6000
)

bpr_add_network_isolated_test(
    NAME application_appearance_store_fallback_contract
    WINDOWS_UNCONTAINED
    COMMAND
        "${CMAKE_COMMAND}"
        "-DBPR_APPLICATION=$<TARGET_FILE:app_better_pull_review>"
        "-DBPR_ROOT=${CMAKE_CURRENT_BINARY_DIR}/appearance-store-fallback-contract"
        "-DBPR_RUN_BUDGET_MS=${bpr_fixture_run_budget_ms}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/appearance_store_fallback_contract.cmake"
)
set_tests_properties(
    application_appearance_store_fallback_contract
    PROPERTIES
        ENVIRONMENT "${bpr_public_path_fixture_environment}"
        PASS_REGULAR_EXPRESSION "APPEARANCE_STORE_FALLBACK_CONTRACT_OK"
)
# Two budgeted real-executable runs: the session switch with a classified diff,
# then a restart against the same obstructed appearance-store path.
bpr_bound_automated_run(
    application_appearance_store_fallback_contract
    TIMEOUT 120
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 2
    RESERVE_MS 4000
)

bpr_add_network_isolated_test(
    NAME binary_dependency_surface
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_binary_dependencies.py"
        $<TARGET_FILE:app_better_pull_review>
)
set_tests_properties(
    binary_dependency_surface
    PROPERTIES
        PASS_REGULAR_EXPRESSION "BINARY_DEPENDENCIES_OK"
        TIMEOUT 15
)

bpr_add_network_isolated_test(
    NAME runtime_process_surface
    WINDOWS_UNCONTAINED
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_runtime_surface.py"
        $<TARGET_FILE:app_better_pull_review>
)
set_tests_properties(
    runtime_process_surface
    PROPERTIES
        PASS_REGULAR_EXPRESSION "RUNTIME_SURFACE_OK"
        TIMEOUT 15
)

bpr_add_network_isolated_test(
    NAME composed_application_hosts
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_composed_hosts.py"
        --profile
        development-fixture
        $<TARGET_FILE:app_better_pull_review>
)
set_tests_properties(
    composed_application_hosts
    PROPERTIES
        TIMEOUT 30
)

bpr_add_network_isolated_test(
    NAME application_render
    WINDOWS_UNCONTAINED
    COMMAND
        $<TARGET_FILE:app_better_pull_review>
        --render-evidence
        "${CMAKE_CURRENT_BINARY_DIR}/foundation-window.png"
        --automated-run-budget-ms
        ${bpr_self_test_budget_ms}
)
set_tests_properties(
    application_render
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1.25"
        PASS_REGULAR_EXPRESSION "RENDER_OK"
)
bpr_bound_automated_run(
    application_render
    TIMEOUT 15
    BUDGET_MS ${bpr_self_test_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)
