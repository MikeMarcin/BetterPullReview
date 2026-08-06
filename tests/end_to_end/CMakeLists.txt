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

add_test(
    NAME application_public_path
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

add_test(
    NAME application_public_path_ghes_cli_unavailable
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

add_test(
    NAME application_worker_executor
    COMMAND test_better_pull_review_worker_executor_tests
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
# One budgeted run — the offscreen probe. The reserve covers the eight
# configuration-failure invocations before it, each of which starts Qt and then
# exits during argument parsing without ever scheduling a run.
bpr_bound_automated_run(
    application_native_appearance_probe_contract
    TIMEOUT 60
    BUDGET_MS ${bpr_native_appearance_budget_ms}
    RUNS 1
    RESERVE_MS 10000
)

add_test(
    NAME application_github_public_path_github_com
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

add_test(
    NAME application_diff_workbench_public_path
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
            "PUBLIC_PATH_OK[\r\n]+GITHUB_FIXTURE_OK:github.com[\r\n]+DIFF_WORKBENCH_OK[\r\n]+DIFF_INTERACTION_EVIDENCE_OK[\r\n]+RENDER_OK[\r\n]+REVIEW_LEDGER_PRESENT_ON_ENTRY:0[\r\n]+REVIEW_LEDGER_OK[\r\n]+WORKBENCH_HOST_COMPOSED_OK[\r\n]+APPEARANCE_HOST_COMPOSED_OK"
)
bpr_bound_automated_run(
    application_diff_workbench_public_path
    TIMEOUT 60
    BUDGET_MS ${bpr_fixture_run_budget_ms}
    RUNS 1
    RESERVE_MS 2000
)

add_test(
    NAME application_diff_workbench_shutdown_public_path
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
add_test(
    NAME application_automated_run_budget
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

add_test(
    NAME application_github_public_path_ghes_3_18
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
    add_test(
        NAME application_epoch_persistence_${fixture_test_name}
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

add_test(
    NAME application_epoch_maintenance_public_path
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

add_test(
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

add_test(
    NAME runtime_process_surface
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

add_test(
    NAME composed_application_hosts
    COMMAND
        Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/scripts/verify_composed_hosts.py"
        $<TARGET_FILE:app_better_pull_review>
)
set_tests_properties(
    composed_application_hosts
    PROPERTIES
        PASS_REGULAR_EXPRESSION "COMPOSED_HOSTS_OK"
        TIMEOUT 30
)

add_test(
    NAME application_render
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
