if(NOT DEFINED BPR_APPLICATION)
    message(FATAL_ERROR "BPR_APPLICATION is required")
endif()
if(NOT DEFINED BPR_BEFORE_RENDER OR NOT DEFINED BPR_AFTER_RENDER)
    message(FATAL_ERROR "appearance render paths are required")
endif()

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
    RESULT_VARIABLE offscreen_result
    OUTPUT_VARIABLE offscreen_output
    ERROR_VARIABLE offscreen_error
    TIMEOUT 15
)
if(offscreen_result EQUAL 0)
    message(FATAL_ERROR "offscreen probe was accepted")
endif()
if(
    NOT offscreen_output MATCHES
        "NATIVE_APPEARANCE_SIGNAL_FAILED[\r\n]+NATIVE_APPEARANCE_STATE_FAILED[\r\n]+NATIVE_APPEARANCE_PIXELS_DIFFER_FAILED[\r\n]+NATIVE_APPEARANCE_CHANGED_RENDER_FAILED"
)
    message(
        FATAL_ERROR
        "offscreen probe failure was not complete: "
        "stdout=${offscreen_output} "
        "stderr=${offscreen_error}"
    )
endif()
if(EXISTS "${BPR_BEFORE_RENDER}" OR EXISTS "${BPR_AFTER_RENDER}")
    message(FATAL_ERROR "rejected offscreen probe produced evidence")
endif()

message("NATIVE_APPEARANCE_PROBE_CONTRACT_OK")
