cmake_minimum_required(VERSION 4.4.2)

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_THEME BPR_RUN_BUDGET_MS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

math(EXPR bpr_phase_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")

function(run_phase label output_name)
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -E
            env
            "QT_QPA_PLATFORM=offscreen"
            "${BPR_APPLICATION}"
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
            ${ARGN}
        RESULT_VARIABLE phase_result
        OUTPUT_VARIABLE phase_output
        ERROR_VARIABLE phase_error
        TIMEOUT ${bpr_phase_timeout}
    )
    if(NOT phase_result EQUAL 0)
        message(FATAL_ERROR
            "${label} process failed (${phase_result})\n"
            "stdout:\n${phase_output}\n"
            "stderr:\n${phase_error}"
        )
    endif()
    set(${output_name} "${phase_output}" PARENT_SCOPE)
endfunction()

function(require_literal label output marker)
    string(FIND "${output}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "${label} omitted ${marker}\nstdout:\n${output}")
    endif()
endfunction()

function(require_regex label output pattern)
    string(REGEX MATCH "${pattern}" matched "${output}")
    if(matched STREQUAL "")
        message(FATAL_ERROR
            "${label} did not match ${pattern}\nstdout:\n${output}"
        )
    endif()
endfunction()

# [[BPR-0875]]. The data root moves inside an owned parent, because the build
# tree this course roots in is writable by other principals and the agent
# workspace neighbourhood refuses such a parent.
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")
bpr_private_data_root(BPR_ROOT)

file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${BPR_ROOT}")
# The root doubles as the profile registry root since BPR-0695 (47de7ae8),
# which refuses a root it did not narrow itself (BPR-0723).
bpr_narrow_owner_private("${BPR_ROOT}")
file(COPY_FILE "${BPR_THEME}" "${BPR_ROOT}/theme.bprtheme")

# Process one uses the real fixture-selected workbench and the real local-file
# import path. The application probe refuses success unless preview and cancel
# wrote nothing, commit wrote exactly once, and the selected comparison kept
# its result, context, semantic selection, span identities, classification
# count, comparison position, semantic index, and measured geometry.
run_phase(
    "theme customization"
    customization_output
    --github-fixture
    github-com
    --epoch-fixture-root
    "${BPR_ROOT}"
    --epoch-fixture-phase
    single
    --diff-workbench-probe
    --diff-interaction-evidence
    "${BPR_ROOT}/diff-evidence"
    --appearance-theme-customization-probe
    "${BPR_ROOT}/theme.bprtheme"
    --render-evidence
    "${BPR_ROOT}/customized-window.png"
)
foreach(marker IN ITEMS
    "PUBLIC_PATH_OK"
    "DIFF_WORKBENCH_OK"
    "SYNTAX_THEME_REPAINT_OK"
    "APPEARANCE_THEME_CUSTOMIZATION_OK"
    "APPEARANCE_THEME_PROBE_STAGE:export-and-directory-refusal-acknowledged"
    "APPEARANCE_THEME_PROBE_STAGE:manual-preview"
    "APPEARANCE_THEME_PROBE_STAGE:manual-cancelled"
    "APPEARANCE_THEME_PROBE_STAGE:palette-reset-cleared-preview"
    "APPEARANCE_THEME_PROBE_MANUAL_FACTS:11111111"
    "APPEARANCE_THEME_PROBE_ADOPTION_FACTS:11111111"
    "APPEARANCE_THEME_PROBE_STORED_ADOPTION_FACTS:111111111"
    "APPEARANCE_THEME_PROBE_EQUAL_COMMIT_FACTS:111111111"
    "APPEARANCE_THEME_PROBE_IDENTITY_FACTS:11111111111"
    "APPEARANCE_THEME_PROBE_FIRST_FACTS:1111111"
    "APPEARANCE_SETTINGS:system:4"
    "APPEARANCE_APPLIED:#000000:#000000:from-profile"
    "APPEARANCE_PERSISTENCE_AVAILABLE"
    "APPEARANCE_DURABLE_WRITES:1"
)
    require_literal("theme customization" "${customization_output}" "${marker}")
endforeach()
require_regex(
    "theme customization"
    "${customization_output}"
    "SYNTAX_CLASSIFICATION:[1-9][0-9]*:[1-9][0-9]*"
)
require_regex(
    "theme customization"
    "${customization_output}"
    "APPEARANCE_OBSERVED:[a-z]+:[a-z-]+:[1-9][0-9]*"
)

# [[BPR-1479]]. Export writes a version-2 document: a base line naming the
# theme the colours were kept on, then every role. BPR_THEME is a version-1
# document. A role it predates was filled at import from its own base, which
# its code background makes Dark, so BPR_THEME_FILLED_ROLES lists those roles
# with Dark's built-in colours ("role #rrggbbaa", comma-separated, in
# vocabulary order). The export must carry them as kept, never unassigned.
string(REGEX MATCH
    "APPEARANCE_THEME_PROBE_CUSTOMISED_VARIANT:([a-z_]+)"
    variant_line
    "${customization_output}"
)
if(variant_line STREQUAL "")
    message(FATAL_ERROR
        "theme customization omitted APPEARANCE_THEME_PROBE_CUSTOMISED_VARIANT\n"
        "stdout:\n${customization_output}"
    )
endif()
set(customised_variant "${CMAKE_MATCH_1}")
file(READ "${BPR_ROOT}/theme.bprtheme" original_theme)
string(FIND "${original_theme}" "bpr-theme 1\n" signature_at)
if(NOT signature_at EQUAL 0)
    message(FATAL_ERROR "BPR_THEME must be a version-1 theme document")
endif()
string(SUBSTRING "${original_theme}" 12 -1 original_roles)
set(expected_theme "bpr-theme 2\nbase ${customised_variant}\n${original_roles}")
if(DEFINED BPR_THEME_FILLED_ROLES)
    string(REPLACE "," ";" filled_roles "${BPR_THEME_FILLED_ROLES}")
    foreach(filled IN LISTS filled_roles)
        string(APPEND expected_theme "${filled}\n")
    endforeach()
endif()
file(READ "${BPR_ROOT}/theme.bprtheme.exported.bprtheme" exported_theme)
if(NOT expected_theme STREQUAL exported_theme)
    message(FATAL_ERROR
        "Export must write the kept palette under its base\n"
        "expected:\n${expected_theme}\n"
        "exported:\n${exported_theme}"
    )
endif()
message("APPEARANCE_THEME_EXPORT_BASE:${customised_variant}")

# Process two opens the same real appearance database without importing or
# committing anything. The exact black semantic canvas can only come from
# process one's persisted total palette; System and tab width four prove the
# unrelated defaults remain intact. The existing composition-only probe stops
# before sign-in: this restart checks appearance durability, not the CLI setup
# course whose older entry path is tracked by BPR-0940.
run_phase(
    "theme restart"
    restart_output
    --self-test
    --composition-capability-probe
    --network-denied
    --test-application-data-root
    "${BPR_ROOT}"
)
foreach(marker IN ITEMS
    "PUBLIC_PATH_OK"
    "APPEARANCE_SETTINGS:system:4"
    "APPEARANCE_APPLIED:#000000:#000000:from-profile"
    "APPEARANCE_PERSISTENCE_AVAILABLE"
    "APPEARANCE_DURABLE_WRITES:0"
)
    require_literal("theme restart" "${restart_output}" "${marker}")
endforeach()
require_regex(
    "theme restart"
    "${restart_output}"
    "APPEARANCE_OBSERVED:[a-z]+:[a-z-]+:[1-9][0-9]*"
)

# A selected path is not sufficient authority to perform a stream read. On
# POSIX, a FIFO would block an ordinary ifstream open until some unrelated
# writer appeared. The product must identify it from the opened descriptor as
# non-regular, publish the ordinary typed file failure, finish shutdown, and
# exit under its own budget rather than under this harness timeout.
if(UNIX)
    find_program(BPR_MKFIFO_EXECUTABLE mkfifo REQUIRED)
    set(special_root "${BPR_ROOT}-special-file")
    set(special_theme "${special_root}/blocking-theme")
    file(REMOVE_RECURSE "${special_root}")
    file(MAKE_DIRECTORY "${special_root}")
    bpr_narrow_owner_private("${special_root}")
    execute_process(
        COMMAND "${BPR_MKFIFO_EXECUTABLE}" "${special_theme}"
        RESULT_VARIABLE fifo_result
        ERROR_VARIABLE fifo_error
    )
    if(NOT fifo_result EQUAL 0)
        message(FATAL_ERROR "could not create FIFO fixture: ${fifo_error}")
    endif()
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -E
            env
            "QT_QPA_PLATFORM=offscreen"
            "${BPR_APPLICATION}"
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
            --github-fixture
            github-com
            --epoch-fixture-root
            "${special_root}"
            --epoch-fixture-phase
            single
            --diff-workbench-probe
            --diff-interaction-evidence
            "${special_root}/diff-evidence"
            --appearance-theme-customization-probe
            "${special_theme}"
        RESULT_VARIABLE special_result
        OUTPUT_VARIABLE special_output
        ERROR_VARIABLE special_error
        TIMEOUT ${bpr_phase_timeout}
    )
    if(special_result EQUAL 0 OR NOT special_result MATCHES "^[0-9]+$")
        message(FATAL_ERROR
            "special-file import did not fail normally (${special_result})\n"
            "stdout:\n${special_output}\n"
            "stderr:\n${special_error}"
        )
    endif()
    foreach(marker IN ITEMS
        "APPEARANCE_THEME_PROBE_STAGE:import-requested"
        "APPEARANCE_THEME_PROBE_STAGE:failed"
        "shutdown_complete=1"
    )
        require_literal("special-file import" "${special_output}" "${marker}")
    endforeach()
    file(REMOVE_RECURSE "${special_root}")
endif()

file(REMOVE_RECURSE "${BPR_ROOT}")
message("APPEARANCE_THEME_CUSTOMIZATION_CONTRACT_OK")
