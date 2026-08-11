cmake_minimum_required(VERSION 4.4.2)

foreach(required IN ITEMS BPR_APPLICATION BPR_ROOT BPR_RUN_BUDGET_MS BPR_SOURCE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

# The attended capture must traverse the shipped drawer selection wire. Keep
# the test-only direct-diff helper compiled so the runtime counter can prove it
# was available but unused.
file(READ "${BPR_SOURCE}" bpr_application_source)
string(FIND
    "${bpr_application_source}"
    "auto detail::github_application_fixture_access::exercise_uiforetw_selected_diff("
    bpr_trace_start
)
string(FIND
    "${bpr_application_source}"
    "auto detail::github_application_fixture_access::uiforetw_selected_diff_evidence("
    bpr_trace_end
)
if(bpr_trace_start EQUAL -1
        OR bpr_trace_end EQUAL -1
        OR bpr_trace_end LESS_EQUAL bpr_trace_start)
    message(FATAL_ERROR "cannot isolate the UIforETW selected-diff trace")
endif()
math(EXPR bpr_trace_length "${bpr_trace_end} - ${bpr_trace_start}")
string(SUBSTRING
    "${bpr_application_source}"
    ${bpr_trace_start}
    ${bpr_trace_length}
    bpr_trace_source
)
string(FIND "${bpr_trace_source}" "request_fixture_diff" bpr_shortcut)
if(NOT bpr_shortcut EQUAL -1)
    message(FATAL_ERROR "UIforETW trace calls the fixture diff shortcut")
endif()
string(FIND
    "${bpr_application_source}"
    "auto detail::github_application_fixture_access::request_fixture_diff("
    bpr_helper
)
if(bpr_helper EQUAL -1)
    message(FATAL_ERROR "fixture diff helper is absent, making the isolation claim vacuous")
endif()

math(EXPR bpr_phase_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")
set(bpr_evidence_root "${BPR_ROOT}/evidence")
file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${bpr_evidence_root}")

foreach(profile IN ITEMS light dark high-contrast-light high-contrast-dark)
    set(profile_root "${BPR_ROOT}/${profile}")
    set(profile_evidence "${bpr_evidence_root}/${profile}")
    file(MAKE_DIRECTORY "${profile_root}")
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -E
            env
            "QT_QPA_PLATFORM=offscreen"
            "${BPR_APPLICATION}"
            --github-fixture
            uiforetw-74
            --epoch-fixture-root
            "${profile_root}"
            --epoch-fixture-phase
            single
            --uiforetw-selected-diff-evidence
            "${profile_evidence}"
            --diff-evidence-appearance
            "${profile}"
            --automated-run-budget-ms
            "${BPR_RUN_BUDGET_MS}"
        RESULT_VARIABLE probe_result
        OUTPUT_VARIABLE probe_output
        ERROR_VARIABLE probe_error
        TIMEOUT ${bpr_phase_timeout}
    )
    if(NOT probe_result EQUAL 0)
        message(FATAL_ERROR
            "UIforETW ${profile} capture failed (${probe_result})\n"
            "stdout:\n${probe_output}\nstderr:\n${probe_error}"
        )
    endif()
    foreach(marker IN ITEMS
        PUBLIC_PATH_OK
        GITHUB_FIXTURE_OK:uiforetw-74
        UIFORETW_EXACT_COMMIT_PAIR_OK
        UIFORETW_RESOURCE_GESTURE_OK
        UIFORETW_RESOURCE_BLOBS_OK
        UIFORETW_RESOURCE_INTRALINE_OK
        UIFORETW_RESOURCE_ROLE_OK
        UIFORETW_RC_GESTURE_OK
        UIFORETW_RC_BLOBS_OK
        UIFORETW_RC_INTRALINE_OK
        UIFORETW_RC_ROLE_OK
        UIFORETW_FOCUSED_REVIEW_LAYOUT_OK
        UIFORETW_FIXTURE_DIFF_HELPER_UNUSED_OK
        UIFORETW_SELECTED_DIFF_TRACE_OK
        UIFORETW_SELECTED_DIFF_EVIDENCE_OK
    )
        string(FIND "${probe_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "UIforETW ${profile} capture omitted ${marker}\n"
                "stdout:\n${probe_output}"
            )
        endif()
    endforeach()
    foreach(artifact IN ITEMS full-window.png intraline-comparison.png capture-facts.json)
        if(NOT EXISTS "${profile_evidence}/${artifact}")
            message(FATAL_ERROR "UIforETW ${profile} omitted ${artifact}")
        endif()
    endforeach()
    file(READ "${profile_evidence}/capture-facts.json" capture_facts)
    foreach(fact IN ITEMS
        "\"appearance\": \"${profile}\""
        "\"base_blob\": \"b995e189fb58da146945e1fb22380cdf4a7f4e47\""
        "\"head_blob\": \"6335c1b2e14cd5e63142ce1dfc283fcf538fb134\""
        "\"fixture_diff_helper_invocations\": 0"
        "\"schema\": \"bpr.uiforetw_intraline_capture.v3\""
        "\"selected_path\": \"UIforETW/UIforETW.rc\""
    )
        string(FIND "${capture_facts}" "${fact}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "UIforETW ${profile} capture facts omitted ${fact}\n${capture_facts}"
            )
        endif()
    endforeach()

	foreach(interaction IN ITEMS
		previous_file_navigation_exercised
		next_file_navigation_exercised
		previous_change_navigation_exercised
		next_change_navigation_exercised
		files_drawer_toggle_exercised
		conversations_drawer_toggle_exercised
		navigation_row_exact_order
		navigation_actions_icon_only
		navigation_actions_have_native_icons
		navigation_actions_accessible
		navigation_accessible_names_unique
	)
		string(JSON interaction_value GET "${capture_facts}" "${interaction}")
		if(NOT interaction_value)
			message(FATAL_ERROR
				"UIforETW ${profile} compact navigation omitted ${interaction}"
			)
		endif()
	endforeach()

    foreach(hidden IN ITEMS
        files_drawer_visible
        conversations_drawer_visible
        review_details_visible
        epoch_controls_visible
        selected_change_detail_visible
    )
        string(JSON hidden_value GET "${capture_facts}" "${hidden}")
        if(hidden_value)
            message(FATAL_ERROR
                "UIforETW ${profile} focused review left ${hidden} true"
            )
        endif()
    endforeach()

    foreach(dimension IN ITEMS logical_width pixel_width)
        string(JSON value GET "${capture_facts}" "${dimension}")
        if(NOT value EQUAL 1280)
            message(FATAL_ERROR
                "UIforETW ${profile} changed ${dimension}: ${value}"
            )
        endif()
    endforeach()
    foreach(dimension IN ITEMS logical_height pixel_height)
        string(JSON value GET "${capture_facts}" "${dimension}")
        if(NOT value EQUAL 1000)
            message(FATAL_ERROR
                "UIforETW ${profile} changed ${dimension}: ${value}"
            )
        endif()
    endforeach()

    foreach(rect IN ITEMS
        full_window_rect
        navigation_row_rect_in_window
        files_toggle_rect_in_row
        previous_file_rect_in_row
        next_file_rect_in_row
        previous_change_rect_in_row
        next_change_rect_in_row
        review_details_rect_in_row
        private_progress_rect_in_row
        conversations_toggle_rect_in_row
        comparison_rect_in_window
        left_pane_rect_in_window
        right_pane_rect_in_window
        gutter_rect_in_window
        files_drawer_rect_in_splitter
        conversations_drawer_rect_in_splitter
        review_details_rect_in_window
    )
        foreach(component IN ITEMS x y width height)
            string(JSON "${rect}_${component}" GET
                "${capture_facts}" "${rect}" "${component}")
        endforeach()
        if(${rect}_width LESS 1 OR ${rect}_height LESS 1)
            message(FATAL_ERROR
                "UIforETW ${profile} ${rect} is empty"
            )
        endif()
    endforeach()
    if(NOT full_window_rect_x EQUAL 0
            OR NOT full_window_rect_y EQUAL 0
            OR NOT full_window_rect_width EQUAL 1280
            OR NOT full_window_rect_height EQUAL 1000)
        message(FATAL_ERROR
            "UIforETW ${profile} full-window rectangle is not exact"
        )
    endif()
	if(NOT navigation_row_rect_in_window_x EQUAL 0
			OR NOT navigation_row_rect_in_window_width EQUAL 1280
			OR navigation_row_rect_in_window_y LESS 0
			OR navigation_row_rect_in_window_height LESS 1)
		message(FATAL_ERROR
			"UIforETW ${profile} compact navigation row does not span the window"
		)
	endif()
	math(EXPR files_toggle_right
		"${files_toggle_rect_in_row_x} + ${files_toggle_rect_in_row_width}")
	math(EXPR previous_file_right
		"${previous_file_rect_in_row_x} + ${previous_file_rect_in_row_width}")
	math(EXPR next_file_right
		"${next_file_rect_in_row_x} + ${next_file_rect_in_row_width}")
	math(EXPR previous_change_right
		"${previous_change_rect_in_row_x} + ${previous_change_rect_in_row_width}")
	math(EXPR next_change_right
		"${next_change_rect_in_row_x} + ${next_change_rect_in_row_width}")
	math(EXPR review_details_right_in_row
		"${review_details_rect_in_row_x} + ${review_details_rect_in_row_width}")
	math(EXPR private_progress_right
		"${private_progress_rect_in_row_x} + ${private_progress_rect_in_row_width}")
	math(EXPR conversations_toggle_right
		"${conversations_toggle_rect_in_row_x} + ${conversations_toggle_rect_in_row_width}")
	math(EXPR left_edge_gap "${files_toggle_rect_in_row_x}")
	math(EXPR right_edge_gap
		"${navigation_row_rect_in_window_width} - ${conversations_toggle_right}")
	math(EXPR flexible_spacer_width
		"${conversations_toggle_rect_in_row_x} - ${private_progress_right}")
	if(left_edge_gap GREATER 12
			OR right_edge_gap GREATER 12
			OR files_toggle_right GREATER previous_file_rect_in_row_x
			OR previous_file_right GREATER next_file_rect_in_row_x
			OR next_file_right GREATER previous_change_rect_in_row_x
			OR previous_change_right GREATER next_change_rect_in_row_x
			OR next_change_right GREATER review_details_rect_in_row_x
			OR review_details_right_in_row GREATER private_progress_rect_in_row_x
			OR private_progress_right GREATER conversations_toggle_rect_in_row_x
			OR flexible_spacer_width LESS 100)
		message(FATAL_ERROR
			"UIforETW ${profile} compact navigation order/edges/spacer drifted"
		)
	endif()
    math(EXPR review_details_right
        "${review_details_rect_in_window_x} + ${review_details_rect_in_window_width}")
    math(EXPR review_details_bottom
        "${review_details_rect_in_window_y} + ${review_details_rect_in_window_height}")
    if(NOT files_drawer_rect_in_splitter_x EQUAL 0
            OR files_drawer_rect_in_splitter_y LESS 0
            OR conversations_drawer_rect_in_splitter_x LESS 1280
            OR conversations_drawer_rect_in_splitter_y LESS 0
            OR review_details_rect_in_window_x LESS 0
            OR review_details_rect_in_window_y LESS 0
            OR review_details_right GREATER 1280
            OR review_details_bottom GREATER 1000)
        message(FATAL_ERROR
            "UIforETW ${profile} hidden drawer/detail rectangles are internally inconsistent"
        )
    endif()

    string(JSON removed_role GET "${capture_facts}" removed_role)
    string(JSON added_role GET "${capture_facts}" added_role)
    string(LENGTH "${removed_role}" removed_role_length)
    string(LENGTH "${added_role}" added_role_length)
    if(NOT removed_role_length EQUAL 9
            OR NOT added_role_length EQUAL 9
            OR removed_role STREQUAL added_role)
        message(FATAL_ERROR
            "UIforETW ${profile} semantic role colors are missing or aliased"
        )
    endif()

    string(JSON width_ratio GET
        "${capture_facts}" comparison_width_ratio_permille)
    string(JSON height_ratio GET
        "${capture_facts}" comparison_height_ratio_permille)
    string(JSON comparison_width GET
        "${capture_facts}" comparison_rect_in_window width)
    string(JSON comparison_height GET
        "${capture_facts}" comparison_rect_in_window height)
    string(JSON crop_width GET "${capture_facts}" comparison_pixel_width)
    string(JSON crop_height GET "${capture_facts}" comparison_pixel_height)
    math(EXPR computed_width_ratio
        "(${comparison_width} * 1000) / 1280")
    math(EXPR computed_height_ratio
        "(${comparison_height} * 1000) / 1000")
    if(NOT width_ratio EQUAL computed_width_ratio
            OR NOT height_ratio EQUAL computed_height_ratio
            OR computed_width_ratio LESS 750
            OR computed_height_ratio LESS 850
            OR comparison_width LESS 960
            OR comparison_height LESS 850
            OR NOT crop_width EQUAL comparison_width
            OR NOT crop_height EQUAL comparison_height)
        message(FATAL_ERROR
            "UIforETW ${profile} is not a native pane-dominant focused review: "
            "ratio=${width_ratio}x${height_ratio} "
            "computed=${computed_width_ratio}x${computed_height_ratio} "
            "comparison=${comparison_width}x${comparison_height} "
            "crop=${crop_width}x${crop_height}"
        )
    endif()
    math(EXPR comparison_right
        "${comparison_rect_in_window_x} + ${comparison_rect_in_window_width}")
    math(EXPR comparison_bottom
        "${comparison_rect_in_window_y} + ${comparison_rect_in_window_height}")
    math(EXPR left_right
        "${left_pane_rect_in_window_x} + ${left_pane_rect_in_window_width}")
    math(EXPR left_bottom
        "${left_pane_rect_in_window_y} + ${left_pane_rect_in_window_height}")
    math(EXPR gutter_right
        "${gutter_rect_in_window_x} + ${gutter_rect_in_window_width}")
    math(EXPR gutter_bottom
        "${gutter_rect_in_window_y} + ${gutter_rect_in_window_height}")
    math(EXPR right_right
        "${right_pane_rect_in_window_x} + ${right_pane_rect_in_window_width}")
    math(EXPR right_bottom
        "${right_pane_rect_in_window_y} + ${right_pane_rect_in_window_height}")
    if(comparison_rect_in_window_x LESS 0
            OR comparison_rect_in_window_y LESS 0
            OR comparison_right GREATER 1280
            OR comparison_bottom GREATER 1000
            OR left_pane_rect_in_window_x LESS comparison_rect_in_window_x
            OR left_right GREATER gutter_rect_in_window_x
            OR gutter_right GREATER right_pane_rect_in_window_x
            OR right_right GREATER comparison_right
            OR left_pane_rect_in_window_y LESS comparison_rect_in_window_y
            OR right_pane_rect_in_window_y LESS comparison_rect_in_window_y
            OR gutter_rect_in_window_y LESS comparison_rect_in_window_y
            OR left_bottom GREATER comparison_bottom
            OR right_bottom GREATER comparison_bottom
            OR gutter_bottom GREATER comparison_bottom)
        message(FATAL_ERROR
            "UIforETW ${profile} pane/gutter rectangles escape or overlap their comparison"
        )
    endif()

    string(JSON left_offset GET "${capture_facts}" left_horizontal_offset)
    string(JSON right_offset GET "${capture_facts}" right_horizontal_offset)
    string(JSON selected_change GET "${capture_facts}" selected_change_index)
    if(left_offset LESS 1 OR right_offset LESS 1 OR NOT selected_change EQUAL 1)
        message(FATAL_ERROR
            "UIforETW ${profile} did not reveal and select the exact replacement: "
            "offsets=${left_offset}/${right_offset} change=${selected_change}"
        )
    endif()

    string(JSON selected_segment GET "${capture_facts}" selected_segment)
    string(JSON selected_group GET "${capture_facts}" selected_group)
    string(LENGTH "${selected_segment}" selected_segment_length)
    string(LENGTH "${selected_group}" selected_group_length)
    if(NOT selected_segment_length EQUAL 64 OR NOT selected_group_length EQUAL 64)
        message(FATAL_ERROR
            "UIforETW ${profile} selected identities are not exact digests"
        )
    endif()

    foreach(target IN ITEMS replacement_28_to_24 replacement_42_to_36)
        string(JSON target_segment GET "${capture_facts}" "${target}" segment)
        string(JSON target_group GET "${capture_facts}" "${target}" group)
        string(JSON target_selected GET
            "${capture_facts}" "${target}" selected_observed)
        string(JSON target_change GET
            "${capture_facts}" "${target}" change_index)
        string(LENGTH "${target_segment}" target_segment_length)
        string(LENGTH "${target_group}" target_group_length)
        if(NOT target_selected
                OR NOT target_segment_length EQUAL 64
                OR NOT target_group_length EQUAL 64)
            message(FATAL_ERROR
                "UIforETW ${profile} ${target} was not selected with exact identity"
            )
        endif()
        foreach(side IN ITEMS left right)
            string(JSON target_width GET
                "${capture_facts}" "${target}" "${side}_rect_in_pane" width)
            string(JSON target_height GET
                "${capture_facts}" "${target}" "${side}_rect_in_pane" height)
            string(JSON target_x GET
                "${capture_facts}" "${target}" "${side}_rect_in_pane" x)
            string(JSON target_y GET
                "${capture_facts}" "${target}" "${side}_rect_in_pane" y)
            string(JSON role_pixels GET
                "${capture_facts}" "${target}" "${side}_role_pixels")
            string(JSON selected_role_pixels GET
                "${capture_facts}" "${target}" "selected_${side}_role_pixels")
            math(EXPR target_right "${target_x} + ${target_width}")
            math(EXPR target_bottom "${target_y} + ${target_height}")
            if(side STREQUAL "left")
                set(pane_width "${left_pane_rect_in_window_width}")
                set(pane_height "${left_pane_rect_in_window_height}")
            else()
                set(pane_width "${right_pane_rect_in_window_width}")
                set(pane_height "${right_pane_rect_in_window_height}")
            endif()
            if(target_width LESS 1
                    OR target_height LESS 1
                    OR target_x LESS 0
                    OR target_y LESS 0
                    OR target_right GREATER pane_width
                    OR target_bottom GREATER pane_height
                    OR role_pixels LESS 1
                    OR selected_role_pixels LESS 1)
                message(FATAL_ERROR
                    "UIforETW ${profile} ${target} ${side} is not visibly painted: "
                    "rect=${target_x},${target_y},${target_width}x${target_height} "
                    "pixels=${role_pixels}/${selected_role_pixels}"
                )
            endif()
        endforeach()
        foreach(component IN ITEMS x y width height)
            string(JSON connector_bounds_${component} GET
                "${capture_facts}" "${target}" connector bounds "${component}")
        endforeach()
        foreach(point IN ITEMS start end)
            foreach(component IN ITEMS x y)
                string(JSON connector_${point}_${component} GET
                    "${capture_facts}" "${target}" connector "${point}" "${component}")
            endforeach()
        endforeach()
        math(EXPR connector_bounds_right
            "${connector_bounds_x} + ${connector_bounds_width}")
        math(EXPR connector_bounds_bottom
            "${connector_bounds_y} + ${connector_bounds_height}")
        if(connector_bounds_width LESS 1
                OR connector_bounds_height LESS 1
                OR connector_bounds_x LESS 0
                OR connector_bounds_y LESS 0
                OR connector_bounds_right GREATER gutter_rect_in_window_width
                OR connector_bounds_bottom GREATER gutter_rect_in_window_height
                OR NOT connector_start_x EQUAL 0
                OR NOT connector_end_x EQUAL gutter_rect_in_window_width
                OR connector_start_y LESS connector_bounds_y
                OR connector_start_y GREATER connector_bounds_bottom
                OR connector_end_y LESS connector_bounds_y
                OR connector_end_y GREATER connector_bounds_bottom)
            message(FATAL_ERROR
                "UIforETW ${profile} ${target} connector coordinates are incomplete or ungrounded"
            )
        endif()
        foreach(fact IN ITEMS
            left_endpoint_at_pane_edge
            right_endpoint_at_pane_edge
            left_target_row_overlap
            right_target_row_overlap
        )
            string(JSON connector_fact GET
                "${capture_facts}" "${target}" connector "${fact}")
            if(NOT connector_fact)
                message(FATAL_ERROR
                    "UIforETW ${profile} ${target} connector omitted ${fact}"
                )
            endif()
        endforeach()
    endforeach()
    string(JSON first_group GET
        "${capture_facts}" replacement_28_to_24 group)
    string(JSON first_segment GET
        "${capture_facts}" replacement_28_to_24 segment)
    string(JSON first_change GET
        "${capture_facts}" replacement_28_to_24 change_index)
    string(JSON first_left_line GET
        "${capture_facts}" replacement_28_to_24 left_line)
    string(JSON first_right_line GET
        "${capture_facts}" replacement_28_to_24 right_line)
    string(JSON second_group GET
        "${capture_facts}" replacement_42_to_36 group)
    string(JSON second_change GET
        "${capture_facts}" replacement_42_to_36 change_index)
    string(JSON second_left_line GET
        "${capture_facts}" replacement_42_to_36 left_line)
    string(JSON second_right_line GET
        "${capture_facts}" replacement_42_to_36 right_line)
    if(NOT selected_group STREQUAL first_group
            OR NOT selected_segment STREQUAL first_segment
            OR first_group STREQUAL second_group
            OR NOT first_change EQUAL 1
            OR NOT second_change EQUAL 2
            OR NOT first_left_line EQUAL 108
            OR NOT first_right_line EQUAL 109
            OR NOT second_left_line EQUAL 110
            OR NOT second_right_line EQUAL 111)
        message(FATAL_ERROR
            "UIforETW ${profile} target selection identities or rows drifted"
        )
    endif()
endforeach()

set(cut_root "${BPR_ROOT}/cut-selection-wire")
execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -E
        env
        "QT_QPA_PLATFORM=offscreen"
        "${BPR_APPLICATION}"
        --github-fixture
        uiforetw-74
        --epoch-fixture-root
        "${cut_root}"
        --epoch-fixture-phase
        single
        --uiforetw-selected-diff-evidence
        "${cut_root}/evidence"
        --diff-evidence-appearance
        light
        --uiforetw-selected-diff-cut-selection-wire
        --automated-run-budget-ms
        "${BPR_RUN_BUDGET_MS}"
    RESULT_VARIABLE cut_result
    OUTPUT_VARIABLE cut_output
    ERROR_VARIABLE cut_error
    TIMEOUT ${bpr_phase_timeout}
)
if(cut_result EQUAL 0)
    message(FATAL_ERROR
        "UIforETW selected-diff trace passed with the real Qt wire cut\n"
        "stdout:\n${cut_output}\nstderr:\n${cut_error}"
    )
endif()
foreach(marker IN ITEMS
    UIFORETW_RESOURCE_GESTURE_OK
    UIFORETW_FIXTURE_DIFF_HELPER_UNUSED_OK
    UIFORETW_SELECTED_DIFF_TRACE_FAILED
)
    string(FIND "${cut_output}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "UIforETW cut-wire run omitted ${marker}\nstdout:\n${cut_output}"
        )
    endif()
endforeach()

message("UIFORETW_SELECTED_DIFF_CONTRACT_OK")
