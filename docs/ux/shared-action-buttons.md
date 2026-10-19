# Shared action button migration inventory

RFC-BPR-0087, initial inventory on main baseline `74ef7f4fb`, updated for the
first source slice. The scope is text choices and secondary actions comparable
to the accepted BPR-1577 Open chip. This inventory is not a claim of completed
app-wide migration: **Pending** entries belong to BPR-1660. Native constructors
are factories as well as individual buttons; their count is not a count of
runtime controls.

The scan covers all 46 Qt translation units containing native push/tool-button
construction or a direct custom subclass. Also retain the established
`disclosure_button`, `menu_tool_button`, `drawer_section_header`, info glyphs,
switches, palette swatches, review-modality segments, `updates_disclosure`,
`review_progress_chip`, `home_row_action` and window caption buttons. The
destructive `question_button`/`danger_tool_button` keep their role colours and
confirmation behavior. QDialogButtonBox and
QMessageBox standard/default/destructive controls keep their platform behavior.

## Reference and first slice

- `counts_mode_button` keeps its measured mark, content and exclusive keyboard
  behavior; `button_surface` owns the common chip/focus painting.
- `quiet_tool_button` opts in text choices and ordinary secondary actions; it
  preserves the underlying QToolButton interaction and accessibility. Menus,
  arrows, icon-only and stacked icon/text controls fall back to their native
  specialized presentation.
- Review and Comments use it for their option and Reset factories. Appearance
  uses it for tab-width choices. Links, checkboxes and numeric fields retain
  their existing purpose and presentation.
- Push-button migration remains explicit future work. Never change a
  QPushButton into a QToolButton just to reuse this painter.

## Surfaces

| Translation unit | Disposition and controls |
|---|---|
| [agent_launch_panel.cpp](../../src/adapters/qt/src/agent_launch_panel.cpp) | Pending: existing-checkout choice and run controls; preserve Start/Stop intent and eligibility. |
| [agent_tab_view.cpp](../../src/adapters/qt/src/agent_tab_view.cpp) | Pending: Context, Back, note/finding actions and banner secondaries. Retain custom menu word and primary Ask/review. |
| [appearance_settings_panel.cpp](../../src/adapters/qt/src/appearance_settings_panel.cpp) | First slice: tab-width choices migrated. Retain deliberately flat card-header actions and primary Keep changes; reassess secondary push wrappers in rollout. |
| [claude_wrapper_panel.cpp](../../src/adapters/qt/src/claude_wrapper_panel.cpp) | Pending: Choose and Save actions; preserve default/save roles. |
| [colour_role_list.cpp](../../src/adapters/qt/src/colour_role_list.cpp) | Retain: colour-swatch picker is an icon/colour control, not a text action. |
| [conversation_finder.cpp](../../src/adapters/qt/src/conversation_finder.cpp) | Pending: empty-state text action. Retain icon-only Clear and custom filter chips. |
| [conversation_open_card.cpp](../../src/adapters/qt/src/conversation_open_card.cpp) | Pending: any text secondaries from shared action factory; retain post footer icon tools, badges, links and primary Save. |
| [device_authorization_panel.cpp](../../src/adapters/qt/src/device_authorization_panel.cpp) | Pending: Open browser, token alternative and Cancel. Retain primary Sign in and copy icon. |
| [diagnostic_settings_panel.cpp](../../src/adapters/qt/src/diagnostic_settings_panel.cpp) | Pending: preference choices and Open logs folder. Retain destructive Clear local diagnostics. |
| [diff_workbench.cpp](../../src/adapters/qt/src/diff_workbench.cpp) | Pending: Open first file and Show files in the idle comparison. |
| [document_tab_strip.cpp](../../src/adapters/qt/src/document_tab_strip.cpp) | Retain: New tab is an icon-only tab-strip affordance. |
| [drawer_parts.cpp](../../src/adapters/qt/src/drawer_parts.cpp) | Retain custom drawer header/icon tools; review text empty-state Action in rollout. |
| [epoch_comparison_control.cpp](../../src/adapters/qt/src/epoch_comparison_control.cpp) | Pending: comparison aliases and history/refresh secondaries. Preserve separate history navigation and cancellation. |
| [feedback_label_controls.cpp](../../src/adapters/qt/src/feedback_label_controls.cpp) | Retain label menu, decoration, state and disclosure variants; review any ordinary text action individually. |
| [home_list.cpp](../../src/adapters/qt/src/home_list.cpp) | Pending: per-row Cancel/Retry text tool; preserve row focus, placement and command ownership. |
| [home_refusal.cpp](../../src/adapters/qt/src/home_refusal.cpp) | Pending: Copy details and secondary recovery actions. Preserve core-selected primary recovery. |
| [insights_window.cpp](../../src/adapters/qt/src/insights_window.cpp) | Pending: Manage cached repositories and Reveal data folder. Retain existing custom range/page navigation. |
| [markdown_body_view.cpp](../../src/adapters/qt/src/markdown_body_view.cpp) | Pending: View source and Copy source. |
| [pending_veil.cpp](../../src/adapters/qt/src/pending_veil.cpp) | Pending: Retry, retaining unavailable/busy projection. |
| [private_composition_editor.cpp](../../src/adapters/qt/src/private_composition_editor.cpp) | Pending: secondaries produced by its button factory. Preserve explicit publication and destructive actions. |
| [private_notes_panel.cpp](../../src/adapters/qt/src/private_notes_panel.cpp) | Retain icon-only New review in the Notes header. |
| [private_review_panel.cpp](../../src/adapters/qt/src/private_review_panel.cpp) | Pending: source-selection/clear actions. Preserve destructive discard/cleanup and primary operations. |
| [profile_data_reset_views.cpp](../../src/adapters/qt/src/profile_data_reset_views.cpp) | Pending: ordinary Cancel/Quit/secondary recovery. Retain destructive reset and default Cancel in confirmations. |
| [profile_management_window.cpp](../../src/adapters/qt/src/profile_management_window.cpp) | Pending: New profile, Rename, retry and Cancel. Preserve primary Open/Save/default behavior. |
| [pull_request_overview_page.cpp](../../src/adapters/qt/src/pull_request_overview_page.cpp) | First slice: Open/All surface painter shared; page-specific mark/content kept. Pending Open check details; retain icon actions and disclosure bars. |
| [refresh_note.cpp](../../src/adapters/qt/src/refresh_note.cpp) | Pending: Retry text action; preserve overlay positioning and timer behavior. |
| [release_maintenance_panel.cpp](../../src/adapters/qt/src/release_maintenance_panel.cpp) | Pending: Update preview and ordinary secondaries. Retain identifying-export and destructive maintenance distinctions. |
| [release_update_panel.cpp](../../src/adapters/qt/src/release_update_panel.cpp) | Pending: View Release and applicable secondary actions; preserve install/restart/default distinctions. |
| [resident_filter.cpp](../../src/adapters/qt/src/resident_filter.cpp) | Pending: Clear filter text action. |
| [review_cycle_promote_sheet.cpp](../../src/adapters/qt/src/review_cycle_promote_sheet.cpp) | Pending: ordinary secondary actions. Preserve explicit promotion/default and dialog semantics. |
| [review_cycle_publish_sheet.cpp](../../src/adapters/qt/src/review_cycle_publish_sheet.cpp) | Pending: Exact Plan, Change, Save Patch, Link Existing and ordinary Cancel. Preserve deliberate publish/resume/primary roles. |
| [review_cycle_share_sheet.cpp](../../src/adapters/qt/src/review_cycle_share_sheet.cpp) | Pending: text secondary actions. Retain share/primary operations, note-code toggle, menus and destructive variants. |
| [review_cycle_view.cpp](../../src/adapters/qt/src/review_cycle_view.cpp) | Pending: Link selection to note and text secondaries. Retain review navigation, note/reply tools and primary Save. |
| [review_draft_editor.cpp](../../src/adapters/qt/src/review_draft_editor.cpp) | Retain custom comment composer toolbar/menu/icon controls; review text-only secondary factory results individually. |
| [review_submission_editor.cpp](../../src/adapters/qt/src/review_submission_editor.cpp) | Retain preflight chips, verdict choices and panel actions with semantic tones, plus saved-comment menus and remove icons. |
| [review_tour_editor.cpp](../../src/adapters/qt/src/review_tour_editor.cpp) | Pending: retry, back, previous/next, add/map and conflict secondaries. Preserve publish/default/destructive roles and overflow menu. |
| [server_card.cpp](../../src/adapters/qt/src/server_card.cpp) | Pending: secondary actions from the card factory. Preserve sign-in/connect primary and sign-out/removal distinctions. |
| [side_chat_panel.cpp](../../src/adapters/qt/src/side_chat_panel.cpp) | Retain promote tool while its specialized agent/draft semantics are reviewed; no automatic variant replacement. |
| [source_find_bar.cpp](../../src/adapters/qt/src/source_find_bar.cpp) | Retain custom find_button: compact source-search/navigation controls. |
| [source_search_panel.cpp](../../src/adapters/qt/src/source_search_panel.cpp) | Retain icon-only collapse/details; review retry text secondaries. |
| [suggestion_composer.cpp](../../src/adapters/qt/src/suggestion_composer.cpp) | Pending: Cancel secondary; preserve primary Save suggestion. |
| [workbench_drawer.cpp](../../src/adapters/qt/src/workbench_drawer.cpp) | Retain Collapse icon/disclosure control. |
| [workbench_landing.cpp](../../src/adapters/qt/src/workbench_landing.cpp) | Pending: secondary/tertiary landing actions. Preserve primary and Close review distinctions. |
| [workbench_strip.cpp](../../src/adapters/qt/src/workbench_strip.cpp) | Retain toolbar/navigation/icon controls. |
| [workbench_window.cpp](../../src/adapters/qt/src/workbench_window.cpp) | First slice: Review/Comments choices and group Reset factory migrated. Pending remaining Settings, Home, repositories and composition-page secondaries. Retain toolbar/icons, links, primary setup/add, destructive remove and confirmations. |
| [writing_hooks_panel.cpp](../../src/adapters/qt/src/writing_hooks_panel.cpp) | Pending: secondary actions from the shared text-button factory; preserve save/reset intent. |

## Completion rule

Before BPR-1660 closes, resolve every Pending entry into migration or a
source-backed intentional variant reason, including dynamic factory results.
Run the affected public composition checks and inspect full-window renders,
not just individual-button rasters. Record style, theme, scale and platform
limits in the RFC verification ledger.
