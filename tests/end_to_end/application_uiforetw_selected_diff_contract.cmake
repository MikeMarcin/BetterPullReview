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

include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")
math(EXPR bpr_phase_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")
# [[BPR-0875]]. The data root moves inside an owned parent, because the build
# tree this course roots in is writable by other principals and the agent
# workspace neighbourhood refuses such a parent.
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")
bpr_private_data_root(BPR_ROOT)

# [[BPR-1036]]. The comparison's height floors, in pixels of the 1000-pixel
# capture window and so in thousandths of it, asserted here on their own as
# well as in the capture. [[BPR-1130]]: RFC-BPR-0052's Updates line folds, a
# new window starts folded, and open its timeline is one 34-pixel row with no
# caption, so the stack is taken both ways. Measured on macOS the comparison is
# 879 folded and 845 open (829 with [[BPR-1129]]'s 50-pixel captioned line). On
# Linux [[BPR-1036]]'s stack gave 834 with the captioned line, so 884 and 850,
# derived until the Linux station measures them. Windows has no menu bar row
# and keeps the owner's 850 budget ([[BPR-0246]]) for the open line, so 884
# folded, unmeasured until the Windows station runs this course. [[BPR-1132]]:
# the comparison's header is the 28-pixel document tab strip, in place of the
# diff status row and its spacing: 872 folded and 838 open on macOS as
# measured, 876 and 842 on Linux derived from the same stack, and on Windows 884
# and the owner's 850.
if(CMAKE_HOST_APPLE)
    set(bpr_comparison_open_height_floor 838)
    set(bpr_comparison_height_floor 872)
elseif(CMAKE_HOST_WIN32)
    set(bpr_comparison_open_height_floor 850)
    set(bpr_comparison_height_floor 884)
else()
    set(bpr_comparison_open_height_floor 842)
    set(bpr_comparison_height_floor 876)
endif()

set(bpr_evidence_root "${BPR_ROOT}/evidence")
file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${bpr_evidence_root}")

foreach(profile IN ITEMS light dark high-contrast-light high-contrast-dark)
    set(profile_root "${BPR_ROOT}/${profile}")
    set(profile_evidence "${bpr_evidence_root}/${profile}")
    file(MAKE_DIRECTORY "${profile_root}")
    # The root is the profile registry root since BPR-0695 (47de7ae8), and
    # the registry refuses a root it did not narrow itself. A pre-created
    # 0755 root fails startup before any marker prints (BPR-0723).
    bpr_narrow_owner_private("${profile_root}")
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

    # BPR-0390. The one question no marker asked: is the comparison on screen
    # actually classified? Every composition marker above stayed green while both
    # panes rendered monochrome, because they report that a classifier exists and
    # that some classification was once adopted — never that this comparison
    # carries spans now.
    #
    # Span *count*, not presence, and the shown flag with it. Presence is too
    # weak: `definition_for_path` falls back to plain text rather than failing, so
    # a pane can hold a non-null span list and be entirely uncoloured. And a
    # count of zero is ambiguous without the shown flag, because "nothing is
    # displayed" is not a defect while "displayed and uncoloured" is.
    #
    # The left pane only. The right pane of this course's final comparison is
    # uncoloured, and BPR-0390 measured why: that blob's classification succeeds
    # and is refused at `claim` because the course shuts down before its terminal
    # lands. A reviewer at the keyboard waits and sees it colour; this course does
    # not. Binding `1:1` here would assert something that is false of the course.
    if(
        NOT probe_output MATCHES
            "SYNTAX_DISPLAYED_SPANS:([0-9]+):([0-9]+):([0-9]+):1"
    )
        message(FATAL_ERROR
            "UIforETW ${profile} displayed no comparison to classify\n"
            "stdout:\n${probe_output}"
        )
    endif()
    if("${CMAKE_MATCH_1}" STREQUAL "0")
        message(FATAL_ERROR
            "UIforETW ${profile} rendered the selected comparison unclassified: "
            "the left pane carries no token spans for a file whose grammar is "
            "embedded\nstdout:\n${probe_output}"
        )
    endif()

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
        "\"schema\": \"bpr.uiforetw_intraline_capture.v7\""
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
		repositories_drawer_toggle_exercised
		title_row_exact_order
		review_bar_exact_order
		review_details_in_comparison_menu
		navigation_actions_icon_only
		navigation_actions_have_native_icons
		navigation_icons_palette_aware
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

	if(profile MATCHES "^high-contrast-")
		set(normal_icon_contrast_floor 700)
	else()
		set(normal_icon_contrast_floor 450)
	endif()
	foreach(navigation_icon IN ITEMS
		previous_file
		next_file
		previous_change
		next_change
	)
		foreach(field IN ITEMS background normal_foreground disabled_foreground)
			string(JSON color GET
				"${capture_facts}" navigation_icon_contrast
				"${navigation_icon}" "${field}")
			string(LENGTH "${color}" color_length)
			if(NOT color_length EQUAL 9)
				message(FATAL_ERROR
					"UIforETW ${profile} ${navigation_icon} ${field} is not an exact ARGB colour"
				)
			endif()
		endforeach()
		string(JSON normal_pixels GET
			"${capture_facts}" navigation_icon_contrast
			"${navigation_icon}" normal_foreground_pixels)
		string(JSON disabled_pixels GET
			"${capture_facts}" navigation_icon_contrast
			"${navigation_icon}" disabled_foreground_pixels)
		string(JSON normal_contrast GET
			"${capture_facts}" navigation_icon_contrast
			"${navigation_icon}" normal_contrast_hundredths)
		string(JSON disabled_contrast GET
			"${capture_facts}" navigation_icon_contrast
			"${navigation_icon}" disabled_contrast_hundredths)
		string(JSON states_distinct GET
			"${capture_facts}" navigation_icon_contrast
			"${navigation_icon}" states_distinct)
		if(normal_pixels LESS 1
			OR disabled_pixels LESS 1
			OR normal_contrast LESS normal_icon_contrast_floor
			OR disabled_contrast LESS 300
			OR NOT states_distinct)
			message(FATAL_ERROR
				"UIforETW ${profile} ${navigation_icon} is unreadable: "
				"pixels=${normal_pixels}/${disabled_pixels} "
				"contrast=${normal_contrast}/${disabled_contrast} "
				"distinct=${states_distinct}"
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
        title_row_rect_in_window
        review_bar_rect_in_window
        files_toggle_rect_in_title_row
        conversations_toggle_rect_in_title_row
        hide_drawers_rect_in_title_row
        comparing_rect_in_bar
        previous_change_rect_in_bar
        next_change_rect_in_bar
        previous_file_rect_in_bar
        next_file_rect_in_bar
        private_progress_rect_in_bar
        compose_review_rect_in_bar
        comment_rect_in_bar
        comparison_rect_in_window
        left_pane_rect_in_window
        right_pane_rect_in_window
        gutter_rect_in_window
        files_drawer_rect_in_splitter
        conversations_drawer_rect_in_splitter
        repositories_drawer_rect_in_splitter
        review_details_rect_in_window
        updates_line_open_rect_in_window
        document_tabs_rect_in_window
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
	# [[BPR-1129]]. RFC-BPR-0052's two rows: the title row spans the window,
	# and the review bar spans it directly beneath, at 30 and 28 pixels.
	math(EXPR title_row_bottom
		"${title_row_rect_in_window_y} + ${title_row_rect_in_window_height}")
	if(NOT title_row_rect_in_window_x EQUAL 0
			OR NOT title_row_rect_in_window_width EQUAL 1280
			OR title_row_rect_in_window_y LESS 0
			OR NOT title_row_rect_in_window_height EQUAL 30
			OR NOT review_bar_rect_in_window_x EQUAL 0
			OR NOT review_bar_rect_in_window_width EQUAL 1280
			OR NOT review_bar_rect_in_window_y EQUAL title_row_bottom
			OR NOT review_bar_rect_in_window_height EQUAL 28)
		message(FATAL_ERROR
			"UIforETW ${profile} the title row and the review bar do not span the window, one under the other: "
			"title=${title_row_rect_in_window_y}+${title_row_rect_in_window_height} "
			"bar=${review_bar_rect_in_window_y}+${review_bar_rect_in_window_height}"
		)
	endif()
	foreach(control IN ITEMS
		files_toggle_rect_in_title_row
		conversations_toggle_rect_in_title_row
		hide_drawers_rect_in_title_row
		comparing_rect_in_bar
		previous_change_rect_in_bar
		next_change_rect_in_bar
		previous_file_rect_in_bar
		next_file_rect_in_bar
		private_progress_rect_in_bar
		compose_review_rect_in_bar
		comment_rect_in_bar
	)
		math(EXPR ${control}_right "${${control}_x} + ${${control}_width}")
	endforeach()
	# [[BPR-0646]] [[BPR-1129]]. The title row keeps the platform inset for the
	# window controls and the Home button before the files toggle, and the
	# comments toggle, Hide Drawers ([[BPR-1295]]), the divider, account and
	# settings at its right. The edge bound starts after the last drawer control.
	# [[BPR-1033]]. On Windows the caption buttons end the row after settings;
	# their width comes off the right edge first, so the bound below stays the
	# one for the divider, account and settings on every platform. The
	# rectangle is empty where the row has no window controls at its right.
	string(JSON window_controls_width GET
		"${capture_facts}" window_controls_rect_in_title_row width)
	math(EXPR left_edge_gap "${files_toggle_rect_in_title_row_x}")
	math(EXPR right_edge_gap
		"${title_row_rect_in_window_width} - ${hide_drawers_rect_in_title_row_right} - ${window_controls_width}")
	if(left_edge_gap GREATER 160
			OR right_edge_gap GREATER 110
			OR files_toggle_rect_in_title_row_right GREATER conversations_toggle_rect_in_title_row_x
			OR conversations_toggle_rect_in_title_row_right GREATER hide_drawers_rect_in_title_row_x)
		message(FATAL_ERROR
			"UIforETW ${profile} title row order/edges drifted"
		)
	endif()
	# [[BPR-1129]]. The review bar reads, from its left, the comparison's menu,
	# the file pair, the change pair and private progress; Compose review and
	# Comment close its right, after a flexible gap. [[BPR-PENDING-1]]: with no
	# review in progress Compose review's words take that gap's room as far as
	# they fit ("Approve…" at 1280, 35 pixels from private progress on macOS),
	# so the gap is bound by the bar's own group separation, 8 pixels, rather
	# than by the 100 pixels an icon left.
	math(EXPR bar_right_gap
		"${review_bar_rect_in_window_width} - ${comment_rect_in_bar_right}")
	math(EXPR bar_gap_width
		"${compose_review_rect_in_bar_x} - ${private_progress_rect_in_bar_right}")
	if(comparing_rect_in_bar_x GREATER 160
			OR comparing_rect_in_bar_right GREATER previous_file_rect_in_bar_x
			OR previous_file_rect_in_bar_right GREATER next_file_rect_in_bar_x
			OR next_file_rect_in_bar_right GREATER previous_change_rect_in_bar_x
			OR previous_change_rect_in_bar_right GREATER next_change_rect_in_bar_x
			OR next_change_rect_in_bar_right GREATER private_progress_rect_in_bar_x
			OR private_progress_rect_in_bar_right GREATER compose_review_rect_in_bar_x
			OR compose_review_rect_in_bar_right GREATER comment_rect_in_bar_x
			OR bar_right_gap GREATER 20
			OR bar_gap_width LESS 8)
		message(FATAL_ERROR
			"UIforETW ${profile} review bar order/edges/gap drifted"
		)
	endif()
    math(EXPR review_details_right
        "${review_details_rect_in_window_x} + ${review_details_rect_in_window_width}")
    math(EXPR review_details_bottom
        "${review_details_rect_in_window_y} + ${review_details_rect_in_window_height}")
    # [[BPR-0646]] [[BPR-0651]] pinned the collapsed drawers as 18-pixel
    # rails in a fixed order: repositories at the splitter's left edge, files
    # past one handle, comments ending at the right edge. [[BPR-0721]] made
    # the rails a setting that is off by default: a collapsed drawer is no
    # column at all, its panel hidden and its handle gone. A hidden panel
    # keeps the geometry it last had, so its rectangle no longer says where
    # anything is; what the focused review now asserts is that each drawer
    # is hidden, no wider than the rail it would be, and that the review
    # details row stays inside the window. The rail geometry itself is
    # pinned with the setting on by `qt_composed_github_scale_*`. The
    # capture records visibility for the two drawers BPR-0646 named; the
    # repositories drawer (BPR-0651) is held to the width bound alone.
    foreach(drawer IN ITEMS files conversations)
        string(JSON drawer_visible GET "${capture_facts}" "${drawer}_drawer_visible")
        if(drawer_visible)
            message(FATAL_ERROR
                "UIforETW ${profile} focused review left the ${drawer} drawer visible"
            )
        endif()
    endforeach()
    if(repositories_drawer_rect_in_splitter_width GREATER 18
            OR files_drawer_rect_in_splitter_width GREATER 18
            OR conversations_drawer_rect_in_splitter_width GREATER 18
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
            OR computed_height_ratio LESS bpr_comparison_height_floor
            OR comparison_width LESS 960
            OR comparison_height LESS bpr_comparison_height_floor
            OR NOT crop_width EQUAL comparison_width
            OR NOT crop_height EQUAL comparison_height)
        message(FATAL_ERROR
            "UIforETW ${profile} is not a native pane-dominant focused review: "
            "ratio=${width_ratio}x${height_ratio} "
            "computed=${computed_width_ratio}x${computed_height_ratio} "
            "comparison=${comparison_width}x${comparison_height} "
            "crop=${crop_width}x${crop_height} "
            "height_floor=${bpr_comparison_height_floor}"
        )
    endif()
    # [[BPR-1130]]. The capture's window started with the Updates line folded
    # and is captured folded; opened through its own command, the timeline is
    # one 34-pixel row directly under the review bar, across the window, and
    # the comparison gives up exactly that row.
    string(JSON initially_folded GET "${capture_facts}" updates_line_initially_folded)
    string(JSON open_comparison_height GET "${capture_facts}" updates_line_open_comparison_height)
    string(JSON open_height_ratio GET "${capture_facts}" updates_line_open_height_ratio_permille)
    math(EXPR review_bar_bottom
        "${review_bar_rect_in_window_y} + ${review_bar_rect_in_window_height}")
    math(EXPR folded_less_open "${comparison_height} - ${open_comparison_height}")
    if(NOT initially_folded
            OR NOT updates_line_open_rect_in_window_x EQUAL 0
            OR NOT updates_line_open_rect_in_window_width EQUAL 1280
            OR NOT updates_line_open_rect_in_window_y EQUAL review_bar_bottom
            OR NOT updates_line_open_rect_in_window_height EQUAL 34
            OR NOT folded_less_open EQUAL updates_line_open_rect_in_window_height
            OR NOT open_height_ratio EQUAL open_comparison_height
            OR open_comparison_height LESS bpr_comparison_open_height_floor)
        message(FATAL_ERROR
            "UIforETW ${profile} the Updates line does not fold as designed: "
            "initially_folded=${initially_folded} "
            "open_line=${updates_line_open_rect_in_window_x},${updates_line_open_rect_in_window_y} "
            "${updates_line_open_rect_in_window_width}x${updates_line_open_rect_in_window_height} "
            "review_bar_bottom=${review_bar_bottom} "
            "folded=${comparison_height} open=${open_comparison_height} "
            "open_height_floor=${bpr_comparison_open_height_floor}"
        )
    endif()
    # [[BPR-1132]]. The comparison's header is the document tab strip: one
    # 28-pixel row directly under the review bar while the line is folded, as
    # the capture is, with the comparison directly beneath it.
    math(EXPR document_tabs_bottom
        "${document_tabs_rect_in_window_y} + ${document_tabs_rect_in_window_height}")
    if(NOT document_tabs_rect_in_window_y EQUAL review_bar_bottom
            OR NOT document_tabs_rect_in_window_height EQUAL 28
            OR NOT comparison_rect_in_window_y EQUAL document_tabs_bottom
            OR document_tabs_rect_in_window_width LESS comparison_rect_in_window_width)
        message(FATAL_ERROR
            "UIforETW ${profile} the comparison's header is not the tab strip: "
            "tabs=${document_tabs_rect_in_window_x},${document_tabs_rect_in_window_y} "
            "${document_tabs_rect_in_window_width}x${document_tabs_rect_in_window_height} "
            "review_bar_bottom=${review_bar_bottom} comparison_top=${comparison_rect_in_window_y}"
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

		foreach(connector_state IN ITEMS selected unselected)
			set(connector_key "${connector_state}_connector")
			string(JSON observed_state GET
				"${capture_facts}" "${target}" "${connector_key}" state)
			foreach(color_field IN ITEMS gutter_background dominant_path_pixel)
				string(JSON connector_color GET
					"${capture_facts}" "${target}" "${connector_key}"
					"${color_field}")
				string(LENGTH "${connector_color}" connector_color_length)
				if(NOT connector_color_length EQUAL 9)
					message(FATAL_ERROR
						"UIforETW ${profile} ${target} ${connector_state} "
						"${color_field} is not exact ARGB"
					)
				endif()
			endforeach()
			string(JSON sampled_pixels GET
				"${capture_facts}" "${target}" "${connector_key}"
				sampled_path_pixels)
			string(JSON contrast_pixels GET
				"${capture_facts}" "${target}" "${connector_key}"
				contrast_pixels)
			string(JSON dominant_pixels GET
				"${capture_facts}" "${target}" "${connector_key}"
				dominant_path_pixels)
			string(JSON contrast_floor GET
				"${capture_facts}" "${target}" "${connector_key}"
				contrast_floor_hundredths)
			string(JSON median_contrast GET
				"${capture_facts}" "${target}" "${connector_key}"
				median_contrast_hundredths)
			math(EXPR twice_contrast_pixels "${contrast_pixels} * 2")
			if(NOT "${observed_state}" STREQUAL "${connector_state}"
					OR sampled_pixels LESS 24
					OR contrast_pixels LESS 24
					OR dominant_pixels LESS 24
					OR NOT contrast_floor EQUAL 300
					OR (connector_state STREQUAL "unselected"
						AND (median_contrast LESS 300
							OR twice_contrast_pixels LESS sampled_pixels)))
				message(FATAL_ERROR
					"UIforETW ${profile} ${target} ${connector_state} "
					"raster is unreadable: state=${observed_state} "
					"pixels=${contrast_pixels}/${sampled_pixels} "
					"dominant=${dominant_pixels} median=${median_contrast}"
				)
			endif()
		endforeach()
		string(JSON selected_background GET
			"${capture_facts}" "${target}" selected_connector gutter_background)
		string(JSON unselected_background GET
			"${capture_facts}" "${target}" unselected_connector gutter_background)
		string(JSON selected_dominant GET
			"${capture_facts}" "${target}" selected_connector dominant_path_pixel)
		string(JSON unselected_dominant GET
			"${capture_facts}" "${target}" unselected_connector dominant_path_pixel)
		if(NOT "${selected_background}" STREQUAL "${unselected_background}"
				OR "${selected_dominant}" STREQUAL "${unselected_dominant}")
			message(FATAL_ERROR
				"UIforETW ${profile} ${target} selected/unselected rasters "
				"are not distinct on one observed gutter"
			)
		endif()
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
