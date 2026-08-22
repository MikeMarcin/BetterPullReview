if(NOT DEFINED BPR_APPLICATION)
    message(FATAL_ERROR "BPR_APPLICATION is required")
endif()
if(NOT DEFINED BPR_BEFORE_RENDER OR NOT DEFINED BPR_AFTER_RENDER)
    message(FATAL_ERROR "appearance render paths are required")
endif()
if(NOT DEFINED BPR_RUN_BUDGET_MS OR "${BPR_RUN_BUDGET_MS}" STREQUAL "")
    message(FATAL_ERROR "BPR_RUN_BUDGET_MS is required")
endif()

function(bpr_compact_cpp_source_slice source_variable start_token end_token output_variable description)
    set(source_text "${${source_variable}}")
    string(FIND "${source_text}" "${start_token}" slice_start)
    string(FIND "${source_text}" "${end_token}" slice_end)
    if(slice_start EQUAL -1 OR slice_end EQUAL -1 OR slice_end LESS_EQUAL slice_start)
        message(FATAL_ERROR "cannot isolate ${description}")
    endif()
    math(EXPR slice_length "${slice_end} - ${slice_start}")
    string(SUBSTRING "${source_text}" ${slice_start} ${slice_length} source_slice)
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" source_slice "${source_slice}")
    string(REGEX REPLACE "//[^\r\n]*" "" source_slice "${source_slice}")
    string(REGEX REPLACE "[ \t\r\n]" "" source_slice "${source_slice}")
    set(${output_variable} "${source_slice}" PARENT_SCOPE)
endfunction()

function(bpr_require_literal_count source token expected description)
    string(LENGTH "${source}" source_length)
    string(LENGTH "${token}" token_length)
    string(REPLACE "${token}" "" source_without_token "${source}")
    string(LENGTH "${source_without_token}" source_without_token_length)
    math(EXPR token_count "(${source_length} - ${source_without_token_length}) / ${token_length}")
    if(NOT token_count EQUAL expected)
        message(FATAL_ERROR "${description} expected ${expected}, found ${token_count}")
    endif()
endfunction()

file(READ "${CMAKE_CURRENT_LIST_DIR}/../../src/app/main.cpp" probe_source)
foreach(
    required_source_fact
    IN ITEMS
    "NATIVE_APPEARANCE_WAITING_FOR_OWNER_RETURN:"
    "NATIVE_APPEARANCE_OWNER_RETURNED\\n"
    "NATIVE_APPEARANCE_OWNER_WINDOW_ACTIVE:"
    "NATIVE_APPEARANCE_OWNER_APPLICATION_ACTIVE_WINDOW:"
    "NATIVE_APPEARANCE_OWNER_EXACT_FOCUS:"
    "NATIVE_APPEARANCE_INITIAL_CAPTURE:"
    "NATIVE_APPEARANCE_CHANGED_CAPTURE:"
    "NATIVE_APPEARANCE_PERSISTENCE:"
    "not_attempted_state_precondition"
    "window_grab_null"
    "normalization_failed"
    "initial_write_failed"
    "changed_write_failed"
    "persisted_readback_failed"
    "OwnerWaitEpisode"
)
    string(FIND "${probe_source}" "${required_source_fact}" source_fact_index)
    if(source_fact_index EQUAL -1)
        message(
            FATAL_ERROR
            "native appearance source omitted diagnostic fact: "
            "${required_source_fact}"
        )
    endif()
endforeach()
if(probe_source MATCHES "NATIVE_APPEARANCE_ACTIVE_FOCUS_")
    message(FATAL_ERROR "native appearance source retained the collapsed focus verdict")
endif()
foreach(unmeasured_hypothesis IN ITEMS "screen_recording_denied" "window_occluded")
    if(probe_source MATCHES "${unmeasured_hypothesis}")
        message(
            FATAL_ERROR
            "native appearance source reports unmeasured platform cause: "
            "${unmeasured_hypothesis}"
        )
    endif()
endforeach()

string(FIND
    "${probe_source}"
    "[[nodiscard]] auto native_appearance_owner_returned("
    owner_predicate_start
)
string(FIND
    "${probe_source}"
    "[[nodiscard]] auto native_appearance_dark_state_passed("
    owner_predicate_end
)
if(
    owner_predicate_start EQUAL -1
    OR owner_predicate_end EQUAL -1
    OR owner_predicate_end LESS_EQUAL owner_predicate_start
)
    message(FATAL_ERROR "cannot isolate the native owner-return predicate")
endif()
math(
    EXPR
    owner_predicate_length
    "${owner_predicate_end} - ${owner_predicate_start}"
)
string(
    SUBSTRING
    "${probe_source}"
    ${owner_predicate_start}
    ${owner_predicate_length}
    owner_predicate_source
)
foreach(
    strict_owner_term
    IN ITEMS
    "aRun.Appearance.InitialFocus != nullptr"
    "aWindow.isActiveWindow()"
    "QApplication::activeWindow() == &aWindow"
    "QApplication::focusWidget() == aRun.Appearance.InitialFocus"
)
    string(FIND "${owner_predicate_source}" "${strict_owner_term}" strict_term_index)
    if(strict_term_index EQUAL -1)
        message(
            FATAL_ERROR
            "native owner-return predicate lost strict term: ${strict_owner_term}"
        )
    endif()
endforeach()
if(owner_predicate_source MATCHES "StatePassed")
    message(FATAL_ERROR "native owner-return predicate depends on appearance state")
endif()
string(
    REGEX REPLACE
    "[ \t\r\n]"
    ""
    owner_predicate_compact
    "${owner_predicate_source}"
)
set(
    expected_owner_predicate
    "returnaRun.Appearance.InitialFocus!=nullptr&&aWindow.isActiveWindow()&&QApplication::activeWindow()==&aWindow&&QApplication::focusWidget()==aRun.Appearance.InitialFocus;"
)
string(
    FIND
    "${owner_predicate_compact}"
    "${expected_owner_predicate}"
    exact_owner_predicate_index
)
if(exact_owner_predicate_index EQUAL -1)
    message(FATAL_ERROR "native owner-return predicate no longer has its exact strict conjunction")
endif()

string(FIND
    "${probe_source}"
    "aRun->Appearance.OwnerReturnEvaluated"
    owner_observation_start
)
string(FIND
    "${probe_source}"
    "if (!native_appearance_owner_returned(window, *aRun))"
    owner_observation_end
)
if(
    owner_observation_start EQUAL -1
    OR owner_observation_end EQUAL -1
    OR owner_observation_end LESS_EQUAL owner_observation_start
)
    message(FATAL_ERROR "cannot isolate the native owner-return observation")
endif()
math(
    EXPR
    owner_observation_length
    "${owner_observation_end} - ${owner_observation_start}"
)
string(
    SUBSTRING
    "${probe_source}"
    ${owner_observation_start}
    ${owner_observation_length}
    owner_observation_source
)
foreach(
    independent_owner_operand
    IN ITEMS
    "OwnerWindowActive[ \t]*=[ \t]*window\\.isActiveWindow\\(\\)"
    "OwnerApplicationActiveWindow[ \t]*=[ \t]*QApplication::activeWindow\\(\\)[ \t]*==[ \t]*&window"
    "OwnerExactFocus[ \t]*=[ \t]*QApplication::focusWidget\\(\\)"
)
    if(NOT owner_observation_source MATCHES "${independent_owner_operand}")
        message(
            FATAL_ERROR
            "native owner-return observation lost independent operand: "
            "${independent_owner_operand}"
        )
    endif()
endforeach()
if(owner_observation_source MATCHES "StatePassed")
    message(FATAL_ERROR "native owner-return observation is contaminated by appearance state")
endif()

bpr_compact_cpp_source_slice(
    probe_source
    "[[nodiscard]] auto native_appearance_dark_state_passed("
    "[[nodiscard]] auto comparison_is_wholly_visible("
    geometry_state_source
    "native appearance dark-state predicate"
)
set(
    exact_zero_geometry_state
    "workbench->statistics().GeometryRebuilds==aRun.Appearance.InitialDiffGeometryRebuilds&&native_appearance_context_passed(aWindow,aRun);"
)
string(FIND "${geometry_state_source}" "${exact_zero_geometry_state}" geometry_state_index)
if(geometry_state_index EQUAL -1)
    message(FATAL_ERROR "native dark-state predicate no longer requires exactly zero geometry rebuilds")
endif()
bpr_require_literal_count(
    "${geometry_state_source}"
    "workbench->statistics().GeometryRebuilds"
    1
    "native dark-state geometry comparison count"
)

bpr_compact_cpp_source_slice(
    probe_source
    "NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_DELTA:"
    "NATIVE_APPEARANCE_DIFF_SELECTION_OK"
    geometry_report_source
    "native appearance geometry verdict"
)
set(
    exact_zero_geometry_verdict
    "<<(aRun.Appearance.DiffGeometryRebuildDelta==0U&&aRun.Appearance.StatePassed?\"NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_OK\\n\":\"NATIVE_APPEARANCE_DIFF_GEOMETRY_REBUILD_FAILED\\n\")"
)
string(FIND "${geometry_report_source}" "${exact_zero_geometry_verdict}" geometry_verdict_index)
if(geometry_verdict_index EQUAL -1)
    message(FATAL_ERROR "native geometry verdict no longer requires exactly zero rebuilds")
endif()
bpr_require_literal_count(
    "${geometry_report_source}"
    "aRun.Appearance.DiffGeometryRebuildDelta"
    2
    "native geometry report delta-reference count"
)

string(REGEX REPLACE "[ \t\r\n]" "" probe_source_compact "${probe_source}")
foreach(
    required_structural_fact
    IN ITEMS
    "if(!native_appearance_owner_returned(window,*aRun)){if(!aRun->Appearance.WaitingForOwnerReturn){++aRun->Appearance.OwnerWaitEpisode;"
    "break;}if(!aRun->Appearance.OwnerReturnReported){QTextStreamstream{stdout};stream<<\"NATIVE_APPEARANCE_OWNER_RETURNED\\n\";"
    "if(windowGrab.isNull()){return{.Disposition=native_appearance_capture_disposition::window_grab_null,};}"
    "if(image.isNull()){return{.Disposition=native_appearance_capture_disposition::normalization_failed,};}"
    "if(!save_new_png(aInitial,aPaths.Initial)){returnnative_appearance_persistence_disposition::initial_write_failed;}"
    "if(!save_new_png(aChanged,aPaths.Changed)){QFile::remove(aPaths.Initial);returnnative_appearance_persistence_disposition::changed_write_failed;}"
    "returnnative_appearance_persistence_disposition::persisted_readback_failed;"
    "returnnative_appearance_persistence_disposition::persisted;"
)
    string(
        FIND
        "${probe_source_compact}"
        "${required_structural_fact}"
        structural_fact_index
    )
    if(structural_fact_index EQUAL -1)
        message(
            FATAL_ERROR
            "native appearance diagnostic control flow changed: "
            "${required_structural_fact}"
        )
    endif()
endforeach()

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
    "NATIVE_APPEARANCE_OWNER_RETURNED"
    "NATIVE_APPEARANCE_OWNER_WINDOW_ACTIVE:"
    "NATIVE_APPEARANCE_OWNER_APPLICATION_ACTIVE_WINDOW:"
    "NATIVE_APPEARANCE_OWNER_EXACT_FOCUS:"
    "NATIVE_APPEARANCE_INITIAL_CAPTURE:"
    "NATIVE_APPEARANCE_CHANGED_CAPTURE:"
    "NATIVE_APPEARANCE_PERSISTENCE:"
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

# BPR-0391. The dark side now waits for the owner's focus to come back before
# it takes its one-shot measurement, bounded only by the run budget. That wait
# is only correct because no automated course can enter it: it lives inside the
# appearance stage, and every course here refuses at `platform_not_cocoa` before
# that stage is entered. If the marker ever appears in one of these runs, an
# automated course is waiting for a person who is not there, and the only thing
# that would end it is budget expiry.
#
# The other half of "cannot hang" is structural and proven above rather than
# here: the budget is checked at the top of every tick, ahead of the stage
# switch that contains the wait, and the startup-budget course is the case that
# shows expiry still produces the complete marker record.
foreach(
    waiting_course
    offscreen
    startup_budget
    cut
)
    if(
        ${waiting_course}_output MATCHES
            "NATIVE_APPEARANCE_WAITING_FOR_OWNER_RETURN"
    )
        message(
            FATAL_ERROR
            "${waiting_course} course entered the owner-return wait: "
            "stdout=${${waiting_course}_output}"
        )
    endif()
endforeach()

message("NATIVE_APPEARANCE_PROBE_CONTRACT_OK")
