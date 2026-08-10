if(NOT DEFINED BPR_APPLICATION)
    message(FATAL_ERROR "BPR_APPLICATION is required")
endif()
if(NOT DEFINED BPR_BEFORE_RENDER OR NOT DEFINED BPR_AFTER_RENDER)
    message(FATAL_ERROR "appearance render paths are required")
endif()
if(NOT DEFINED BPR_RUN_BUDGET_MS OR "${BPR_RUN_BUDGET_MS}" STREQUAL "")
    message(FATAL_ERROR "BPR_RUN_BUDGET_MS is required")
endif()

# A backstop, not the bound. The probe bounds itself at `BPR_RUN_BUDGET_MS` and
# reports what it was doing; this only catches a process that never reached its
# own limit, and is derived from that limit so it can never fire first. The
# configuration-failure cases below keep their own short timeout: each exits
# during argument parsing without ever scheduling a run, so no budget applies.
math(EXPR bpr_probe_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 10")

file(REMOVE "${BPR_BEFORE_RENDER}" "${BPR_AFTER_RENDER}")

function(expect_configuration_failure case_name)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -E
            env
            "QT_QPA_PLATFORM=offscreen"
            "${BPR_APPLICATION}"
            ${ARGN}
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
        TIMEOUT 15
    )
    if(
        probe_result EQUAL 0
        OR NOT probe_output MATCHES
            "^NATIVE_APPEARANCE_CONFIGURATION_FAILED[\r\n]+$"
    )
        message(
            FATAL_ERROR
            "${case_name} did not fail configuration closed: "
            "result=${probe_result} "
            "stdout=${probe_output} "
            "stderr=${probe_error}"
        )
    endif()
endfunction()

expect_configuration_failure(
    "incomplete probe"
    --native-appearance-probe
)
expect_configuration_failure(
    "changed-render path without probe"
    --changed-render-evidence
    "${BPR_AFTER_RENDER}"
)
expect_configuration_failure(
    "GHES probe"
    --github-fixture
    ghes-3.18
    --native-appearance-probe
    --render-evidence
    "${BPR_BEFORE_RENDER}"
    --changed-render-evidence
    "${BPR_AFTER_RENDER}"
)
expect_configuration_failure(
    "same-path probe"
    --github-fixture
    github-com
    --native-appearance-probe
    --render-evidence
    "${BPR_BEFORE_RENDER}"
    --changed-render-evidence
    "${BPR_BEFORE_RENDER}"
)

get_filename_component(
    bpr_render_directory
    "${BPR_BEFORE_RENDER}"
    DIRECTORY
)
get_filename_component(
    bpr_render_filename
    "${BPR_BEFORE_RENDER}"
    NAME
)
expect_configuration_failure(
    "lexically aliased probe"
    --github-fixture
    github-com
    --native-appearance-probe
    --render-evidence
    "${BPR_BEFORE_RENDER}"
    --changed-render-evidence
    "${bpr_render_directory}/./${bpr_render_filename}"
)

file(WRITE "${BPR_BEFORE_RENDER}" "existing-evidence-sentinel")
expect_configuration_failure(
    "existing destination probe"
    --github-fixture
    github-com
    --native-appearance-probe
    --render-evidence
    "${BPR_BEFORE_RENDER}"
    --changed-render-evidence
    "${BPR_AFTER_RENDER}"
)
file(READ "${BPR_BEFORE_RENDER}" existing_destination_contents)
if(NOT existing_destination_contents STREQUAL "existing-evidence-sentinel")
    message(FATAL_ERROR "existing appearance evidence was modified")
endif()
file(REMOVE "${BPR_BEFORE_RENDER}")

if(UNIX)
    set(
        bpr_missing_render_target
        "${bpr_render_directory}/native-appearance-missing-target"
    )
    file(REMOVE "${BPR_BEFORE_RENDER}" "${bpr_missing_render_target}")
    file(
        CREATE_LINK
        "${bpr_missing_render_target}"
        "${BPR_BEFORE_RENDER}"
        SYMBOLIC
        RESULT dangling_link_result
    )
    if(NOT dangling_link_result STREQUAL "0")
        message(FATAL_ERROR "could not create dangling appearance path link")
    endif()
    expect_configuration_failure(
        "dangling-symlink destination probe"
        --github-fixture
        github-com
        --native-appearance-probe
        --render-evidence
        "${BPR_BEFORE_RENDER}"
        --changed-render-evidence
        "${BPR_AFTER_RENDER}"
    )
    if(NOT IS_SYMLINK "${BPR_BEFORE_RENDER}")
        message(FATAL_ERROR "dangling appearance path link was modified")
    endif()
    file(REMOVE "${BPR_BEFORE_RENDER}")

    set(
        bpr_real_render_directory
        "${bpr_render_directory}/native-appearance-real"
    )
    set(
        bpr_link_render_directory
        "${bpr_render_directory}/native-appearance-link"
    )
    file(REMOVE "${bpr_link_render_directory}")
    file(REMOVE_RECURSE "${bpr_real_render_directory}")
    file(MAKE_DIRECTORY "${bpr_real_render_directory}")
    file(
        CREATE_LINK
        "${bpr_real_render_directory}"
        "${bpr_link_render_directory}"
        SYMBOLIC
        RESULT link_result
    )
    if(NOT link_result STREQUAL "0")
        message(FATAL_ERROR "could not create appearance path test link")
    endif()
    expect_configuration_failure(
        "symlinked-directory alias probe"
        --github-fixture
        github-com
        --native-appearance-probe
        --render-evidence
        "${bpr_real_render_directory}/evidence.png"
        --changed-render-evidence
        "${bpr_link_render_directory}/evidence.png"
    )
    file(REMOVE "${bpr_link_render_directory}")
    file(REMOVE_RECURSE "${bpr_real_render_directory}")
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -E
        env
        "QT_QPA_PLATFORM=offscreen"
        "${BPR_APPLICATION}"
        --github-fixture
        github-com
        --native-appearance-probe
        --render-evidence
        "${BPR_BEFORE_RENDER}"
        --changed-render-evidence
        "${BPR_AFTER_RENDER}"
        --automated-run-budget-ms
        "${BPR_RUN_BUDGET_MS}"
    RESULT_VARIABLE offscreen_result
    OUTPUT_VARIABLE offscreen_output
    ERROR_VARIABLE offscreen_error
    TIMEOUT ${bpr_probe_timeout}
)
if(offscreen_result EQUAL 0)
    message(FATAL_ERROR "offscreen probe was accepted")
endif()
foreach(
    expected_marker
    "NATIVE_APPEARANCE_SELECTED_DIFF_READY"
    "NATIVE_APPEARANCE_SELECTED_DIFF_REASON:none"
    "NATIVE_APPEARANCE_SELECTED_DIFF_BINDING:111111"
    "NATIVE_APPEARANCE_SELECTION_WIRE_OK"
    "NATIVE_APPEARANCE_FIXTURE_DIFF_HELPER_UNUSED_OK"
    "NATIVE_APPEARANCE_PRECONDITION_REFUSED:platform_not_cocoa:none"
)
    if(NOT offscreen_output MATCHES "(^|[\r\n])${expected_marker}([\r\n]|$)")
        message(
            FATAL_ERROR
            "offscreen real-selection course missed ${expected_marker}: "
            "stdout=${offscreen_output} stderr=${offscreen_error}"
        )
    endif()
endforeach()

set(
    appearance_verdict_markers
    "APPEARANCE_SYSTEM_FOLLOWS_"
    "APPEARANCE_EXPLICIT_IGNORES_"
    "APPEARANCE_VARIANTS:"
    "NATIVE_APPEARANCE_TRANSITION:"
    "NATIVE_APPEARANCE_GLOBAL_TRANSITION:"
    "NATIVE_APPEARANCE_ACTIVE_FOCUS_"
    "NATIVE_APPEARANCE_SIGNAL_"
    "NATIVE_APPEARANCE_STATE_"
    "NATIVE_APPEARANCE_PIXELS_DIFFER_"
    "NATIVE_APPEARANCE_DIFF_PALETTE_BEFORE:"
    "NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_DELTA:"
    "NATIVE_APPEARANCE_DIFF_SELECTION_"
    "NATIVE_APPEARANCE_CHANGED_RENDER_"
)
foreach(forbidden_marker IN LISTS appearance_verdict_markers)
    if(offscreen_output MATCHES "${forbidden_marker}")
        message(
            FATAL_ERROR
            "offscreen precondition refusal emitted appearance verdict "
            "${forbidden_marker}: stdout=${offscreen_output}"
        )
    endif()
endforeach()
if(EXISTS "${BPR_BEFORE_RENDER}" OR EXISTS "${BPR_AFTER_RENDER}")
    message(FATAL_ERROR "rejected offscreen probe produced evidence")
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -E
        env
        "QT_QPA_PLATFORM=offscreen"
        "${BPR_APPLICATION}"
        --github-fixture
        github-com
        --native-appearance-probe
        --render-evidence
        "${BPR_BEFORE_RENDER}"
        --changed-render-evidence
        "${BPR_AFTER_RENDER}"
        --automated-run-budget-ms
        1
    RESULT_VARIABLE startup_budget_result
    OUTPUT_VARIABLE startup_budget_output
    ERROR_VARIABLE startup_budget_error
    TIMEOUT 15
)
if(startup_budget_result EQUAL 0)
    message(FATAL_ERROR "startup-budget native course was accepted")
endif()
foreach(
    expected_marker
    "NATIVE_APPEARANCE_SELECTED_DIFF_PENDING"
    "NATIVE_APPEARANCE_SELECTED_DIFF_REASON:awaiting_workbench_observation"
    "NATIVE_APPEARANCE_SELECTED_DIFF_BINDING:000000"
    "NATIVE_APPEARANCE_SELECTION_WIRE_FAILED"
    "NATIVE_APPEARANCE_FIXTURE_DIFF_HELPER_UNUSED_OK"
    "NATIVE_APPEARANCE_PRECONDITION_REFUSED:selected_diff_budget_exhausted:awaiting_workbench_observation"
)
    if(
        NOT startup_budget_output
            MATCHES "(^|[\r\n])${expected_marker}([\r\n]|$)"
    )
        message(
            FATAL_ERROR
            "startup-budget course missed ${expected_marker}: "
            "stdout=${startup_budget_output} stderr=${startup_budget_error}"
        )
    endif()
endforeach()
if(
    NOT startup_budget_output
        MATCHES "PUBLIC_PATH_VERDICT:presentation=0 shutdown_complete=1 cleanup_failed=0"
    OR NOT startup_budget_output
        MATCHES "PUBLIC_PATH_BUDGET:expired=1 budget_ms=1"
)
    message(
        FATAL_ERROR
        "startup-budget course did not shut down within its own bound: "
        "stdout=${startup_budget_output} stderr=${startup_budget_error}"
    )
endif()
if(
    startup_budget_output MATCHES
        "NATIVE_APPEARANCE_SELECTED_DIFF_REASON:composition_unavailable"
    OR startup_budget_output MATCHES
        "NATIVE_APPEARANCE_FIXTURE_DIFF_HELPER_UNUSED_FAILED"
    OR startup_budget_output MATCHES "(^|[\r\n])RENDER_(OK|FAILED)([\r\n]|$)"
)
    message(
        FATAL_ERROR
        "startup-budget course fabricated evidence: stdout=${startup_budget_output}"
    )
endif()
foreach(forbidden_marker IN LISTS appearance_verdict_markers)
    if(startup_budget_output MATCHES "${forbidden_marker}")
        message(
            FATAL_ERROR
            "startup-budget refusal emitted appearance verdict "
            "${forbidden_marker}: stdout=${startup_budget_output}"
        )
    endif()
endforeach()
if(EXISTS "${BPR_BEFORE_RENDER}" OR EXISTS "${BPR_AFTER_RENDER}")
    message(FATAL_ERROR "startup-budget native course produced evidence")
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -E
        env
        "QT_QPA_PLATFORM=offscreen"
        "${BPR_APPLICATION}"
        --github-fixture
        github-com
        --native-appearance-probe
        --native-appearance-cut-selection-wire
        --render-evidence
        "${BPR_BEFORE_RENDER}"
        --changed-render-evidence
        "${BPR_AFTER_RENDER}"
        --automated-run-budget-ms
        "${BPR_RUN_BUDGET_MS}"
    RESULT_VARIABLE cut_result
    OUTPUT_VARIABLE cut_output
    ERROR_VARIABLE cut_error
    TIMEOUT ${bpr_probe_timeout}
)
if(cut_result EQUAL 0)
    message(FATAL_ERROR "cut native selection wire was accepted")
endif()
foreach(
    expected_marker
    "NATIVE_APPEARANCE_SELECTED_DIFF_REFUSED"
    "NATIVE_APPEARANCE_SELECTED_DIFF_REASON:selection_wire_unavailable"
    "NATIVE_APPEARANCE_SELECTED_DIFF_BINDING:111110"
    "NATIVE_APPEARANCE_SELECTION_WIRE_FAILED"
    "NATIVE_APPEARANCE_FIXTURE_DIFF_HELPER_UNUSED_OK"
    "NATIVE_APPEARANCE_PRECONDITION_REFUSED:selected_diff_refused:selection_wire_unavailable"
)
    if(NOT cut_output MATCHES "(^|[\r\n])${expected_marker}([\r\n]|$)")
        message(
            FATAL_ERROR
            "cut native selection course missed ${expected_marker}: "
            "stdout=${cut_output} stderr=${cut_error}"
        )
    endif()
endforeach()
foreach(forbidden_marker IN LISTS appearance_verdict_markers)
    if(cut_output MATCHES "${forbidden_marker}")
        message(
            FATAL_ERROR
            "cut selection refusal emitted appearance verdict "
            "${forbidden_marker}: stdout=${cut_output}"
        )
    endif()
endforeach()
if(EXISTS "${BPR_BEFORE_RENDER}" OR EXISTS "${BPR_AFTER_RENDER}")
    message(FATAL_ERROR "cut native selection wire produced evidence")
endif()

message("NATIVE_APPEARANCE_PROBE_CONTRACT_OK")
