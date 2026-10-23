# Shared action button migration inventory

RFC-BPR-0087, initial inventory on main baseline `74ef7f4fb`, updated for
BPR-1659 and BPR-1660, and rebased onto main `4e318b625` on 2026-10-22 (the
[surfaces main added since](#surfaces-added-on-main-after-the-baseline)). The
scope is text choices and secondary actions comparable to the accepted BPR-1577
Open chip. Every entry is migrated or intentionally retained below.
Native constructors are factories as well as individual buttons; their count
is not a count of runtime controls.

The table covers the 46 page/factory translation units in the initial native
push/tool-button construction scan. Dedicated specialized implementations are
accounted for separately below. Also retain the established
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
- `quiet_push_button` uses the same painter beneath QPushButton. It preserves
  push signals, dialog defaults, mnemonics, icons and accessibility; default,
  menu and icon-only controls fall back to native painting. A flat secondary
  retains its bare rest treatment. No push button becomes a tool button.

## Surfaces

| Translation unit | Disposition and controls |
|---|---|
| [agent_launch_panel.cpp](../../src/adapters/qt/src/agent_launch_panel.cpp) | Migrated `ExistingCheckoutButton` and Stop (`CancelButton`). Retain `StartButton`: explicit primary agent launch, not a choice/secondary. |
| [agent_tab_view.cpp](../../src/adapters/qt/src/agent_tab_view.cpp) | Migrated Context, Back to conversation, Draft a comment, Ask follow-up, Add to notes and flat `BannerSecond` (continue old head/log). Retain `BannerFirst`: the banner’s primary Start/Ask again/Stop action; `review` launches a review; `menu_word` is a custom popup. |
| [appearance_settings_panel.cpp](../../src/adapters/qt/src/appearance_settings_panel.cpp) | Tab-width choices use `quiet_tool_button`. Retain `make_header_button` products Theme/Type/Colours/Dif Reset, Import/Export and Cancel changes: the accepted flat card-header/footer language, never defaults. Keep changes is the primary preview confirmation. |
| [claude_wrapper_panel.cpp](../../src/adapters/qt/src/claude_wrapper_panel.cpp) | Migrated Choose. Retain `SaveButton` and `ModelSave`: explicit commits of wrapper/model configuration, the panels’ primary actions. |
| [colour_role_list.cpp](../../src/adapters/qt/src/colour_role_list.cpp) | Retain `swatch`: colour/icon-only picker. It paints a colour, not an action label. |
| [conversation_finder.cpp](../../src/adapters/qt/src/conversation_finder.cpp) | Migrated `makeEmpty`’s sole nonempty action, Show all threads (clears filters). Retain icon-only Clear and `finder_chip`’s custom filter/menu presentation. |
| [conversation_open_card.cpp](../../src/adapters/qt/src/conversation_open_card.cpp) | Migrated reply Cancel. `tool` produces icon-only post tools; footer Copy text/Copy change/Edit/View source are glyph tools; where/badge and main's file-comment source link are source/status affordances. Retain custom `note_button` primary Publish/Send/Save, links and badges. |
| [device_authorization_panel.cpp](../../src/adapters/qt/src/device_authorization_panel.cpp) | Migrated Open browser, Cancel and token alternative. Retain primary Sign in (`BeginButton`) and icon-only Copy code. Browser handoff remains host-owned. |
| [diagnostic_settings_panel.cpp](../../src/adapters/qt/src/diagnostic_settings_panel.cpp) | Migrated the complete Detail/Privacy selector factory (Standard/Verbose, Anonymized/Include locally) and Open logs folder. Retain Clear local diagnostics: destructive, still confirmed before dispatch. |
| [diff_workbench.cpp](../../src/adapters/qt/src/diff_workbench.cpp) | Migrated idle Open first file and Show files. Source/diff rendering and navigation remain unchanged. |
| [document_tab_strip.cpp](../../src/adapters/qt/src/document_tab_strip.cpp) | Retain New tab: icon-only tab-strip affordance. |
| [drawer_parts.cpp](../../src/adapters/qt/src/drawer_parts.cpp) | Migrated `drawer_empty_state::Action` (text beside optional action icon). Retain `drawer_tool`, section/header/collapse tools: dedicated drawer chrome. |
| [epoch_comparison_control.cpp](../../src/adapters/qt/src/epoch_comparison_control.cpp) | Migrated Whole PR/Previous upload/Last reviewed aliases, Load history, Refresh and Cancel. Retain Return to newest as history navigation, Comparing/maintenance popup controls, and QMessageBox confirmation/default buttons. |
| [feedback_label_controls.cpp](../../src/adapters/qt/src/feedback_label_controls.cpp) | Retain the complete label-menu language: `Chip` opens the popup; Plain/Labels choose semantic labelled marks; `make_toggle` produces decoration/state choices; CustomAdd edits the same menu; About is a hyperlink. These stay together with the popup’s arrow-key/Return/Escape selection treatment. |
| [home_list.cpp](../../src/adapters/qt/src/home_list.cpp) | Migrated the real row Cancel/Retry `OpenAction`; it keeps its NoFocus row-owned Escape behavior and placement. Retain painted `home_row_action` primary Open. |
| [home_refusal.cpp](../../src/adapters/qt/src/home_refusal.cpp) | Migrated all `Actions` recovery products and Copy details. The factory explicitly makes bordered, nondefault secondary steps beside Home’s own filled primary; core ordering and consequence descriptions remain authoritative. |
| [insights_window.cpp](../../src/adapters/qt/src/insights_window.cpp) | Migrated Manage cached repositories and Reveal data folder. Retain custom `header_button` range/page/folder navigation and chart controls. |
| [markdown_body_view.cpp](../../src/adapters/qt/src/markdown_body_view.cpp) | Migrated View source/Hide source toggle and Copy source. Native clipboard dispatch and read-only source body remain. |
| [pending_veil.cpp](../../src/adapters/qt/src/pending_veil.cpp) | Migrated Retry. Busy/unavailable eligibility and veil positioning remain projection-owned. |
| [private_composition_editor.cpp](../../src/adapters/qt/src/private_composition_editor.cpp) | Factory migrates Use selected exact target and Cancel composition view. Retain Create candidate, Split into exact drafts, Save revision, Ask agent and Accept as Draft: explicit primary composition/persistence operations. Factory branches on the typed action. |
| [private_notes_panel.cpp](../../src/adapters/qt/src/private_notes_panel.cpp) | Retain New review: icon-only Notes-header tool. |
| [private_review_panel.cpp](../../src/adapters/qt/src/private_review_panel.cpp) | Migrated source/finding selection, Clear selected sources, Refresh history, Inspect provenance/finding and Open local draft. Factory retains Next thread/revision/body/findings page as bounded-history navigation and primary create/save/ask/compose operations. Discard unpublished draft and Free unused empty drafts retain destructive confirmations. |
| [profile_data_reset_views.cpp](../../src/adapters/qt/src/profile_data_reset_views.cpp) | Migrated recovery Quit. Retain all four reset-scope actions, recovery Clear caches/Reset everything, destructive `question_button` confirmation and its safe default Cancel. These are reset/default controls, not neutral secondary chips. |
| [profile_management_window.cpp](../../src/adapters/qt/src/profile_management_window.cpp) | Migrated New profile, Rename, Try again and Cancel. Retain primary Open/New window and Save, including default behavior, row selection and edit/refusal retention. |
| [pull_request_overview_page.cpp](../../src/adapters/qt/src/pull_request_overview_page.cpp) | Open/All retain their accepted page content with shared surface painter. Migrated Open check details and Back to cached overview. Retain icon-only FullView/folder/label-key/NewThreads/NewConversation tools, semantic uncertainty mark and disclosure bars. |
| [refresh_note.cpp](../../src/adapters/qt/src/refresh_note.cpp) | Migrated Retry. Overlay timing, eligibility and placement remain intact. |
| [release_maintenance_panel.cpp](../../src/adapters/qt/src/release_maintenance_panel.cpp) | Migrated Update preview and factory products Export anonymized diagnostics, Prepare diagnostic cleanup, Prepare profile purge and Cancel maintenance. Retain Save in the dialog’s ActionRole, actual Clear/Purge destructive operations, identifying-details export and standard dialog Cancel/default semantics. Factory branches by the existing command index. |
| [release_update_panel.cpp](../../src/adapters/qt/src/release_update_panel.cpp) | Main's redesigned panel: migrated Check Now, Later, Skip This Version, Cancel and Open Release Page. The panel makes its lead command the default, and a default `quiet_push_button` is drawn natively, so a lead Check Now, Cancel or Open Release Page keeps the default look and the button's geometry is refreshed when the lead changes. Retain Download and Install and Restart: explicit primary update/install actions. Factory branches by typed command. |
| [resident_filter.cpp](../../src/adapters/qt/src/resident_filter.cpp) | Migrated Clear filter, retaining the filter dispatch. |
| [review_cycle_promote_sheet.cpp](../../src/adapters/qt/src/review_cycle_promote_sheet.cpp) | Migrated Open Pull Request and Done. Retain Add Notes: explicit promotion/default confirmation. |
| [review_cycle_publish_sheet.cpp](../../src/adapters/qt/src/review_cycle_publish_sheet.cpp) | Migrated Read Exact Plan, Change, Cancel Publishing, Save Patch, Link Existing and ordinary Cancel. Retain primary Review Plan/Publish/resume state and Link confirmation. Flat tertiary export/link actions remain bare at rest with shared feedback. |
| [review_cycle_share_sheet.cpp](../../src/adapters/qt/src/review_cycle_share_sheet.cpp) | Migrated all Ask them to choices, Details, Stop send, Exact Text, Cancel and Show Whole File. Route factory migrates tertiary Save as Bundle, Use in External Agent and Send to Agent routes; retains primary Copy/Save document. Retain Previous/Next/Done in the exact-text navigation dialog, note-code toggle, Replace It/Keep It conflict decision, primary Send/Add as Reply and custom menu/mark tools. |
| [review_cycle_view.cpp](../../src/adapters/qt/src/review_cycle_view.cpp) | Migrated Link selection to note and comment/reply Cancel. Retain review-bar View/Next round navigation, icon note/reply tools and primary Save note/reply/suggestion. |
| [review_draft_editor.cpp](../../src/adapters/qt/src/review_draft_editor.cpp) | Migrated Reapply my text. Conflict factory retains Reload accepted draft because it replaces current edits. Retain Publish/Discard, custom composer toolbar, menus, suggestion mark and icon tools. |
| [review_submission_editor.cpp](../../src/adapters/qt/src/review_submission_editor.cpp) | Migrated the held comment's Convert (RFC-BPR-0090 and BPR-1735's Convert to file comment), a secondary text action that drew the native bezel. Retain semantic preflight chips, verdict choices and toned panel actions; saved-comment menus and the icon-only Delete are specialized review controls. |
| [review_tour_editor.cpp](../../src/adapters/qt/src/review_tour_editor.cpp) | Migrated Load selected tour, Retry local save, description mapping, Preview/Check publication status/Stop publication, writing assistance, Add stop/Suggest outline/Insert skeleton, Use selected lines/Related source/Show in code, and Reaffirm explanation. Factory retains Remove link (destructive); saved/website/local conflict decisions retain replacement/keep distinctions. Retain primary Publish/Preview and publish/Ready next, primary publication Resume recovery, Previous/Next/Next changed navigation, and overflow menu. |
| [server_card.cpp](../../src/adapters/qt/src/server_card.cpp) | Factory migrates Check gh again and Copy details. Retain primary Sign in/Connect/CLI connect/Use for this profile, Sign out and Forget server (credential/removal consequences). Dedicated disclosures remain. |
| [side_chat_panel.cpp](../../src/adapters/qt/src/side_chat_panel.cpp) | Retain Promote reply to draft: the explicit primary conversion action for the selected agent reply, with eligibility and consequence description. |
| [source_find_bar.cpp](../../src/adapters/qt/src/source_find_bar.cpp) | Retain `find_button`: source-search/navigation glyphs and compact toolbar treatment. |
| [source_search_panel.cpp](../../src/adapters/qt/src/source_search_panel.cpp) | Migrated Retry unavailable reads. Retain icon-only Collapse all/Details and the skipped-results disclosure. |
| [suggestion_composer.cpp](../../src/adapters/qt/src/suggestion_composer.cpp) | Migrated Cancel. Retain primary Save suggestion. |
| [workbench_drawer.cpp](../../src/adapters/qt/src/workbench_drawer.cpp) | Retain Collapse: icon/disclosure drawer chrome. |
| [workbench_landing.cpp](../../src/adapters/qt/src/workbench_landing.cpp) | Migrated Secondary/Tertiary actions. Retain Primary (default way back/sign in) and Close review, the distinct bordered review-closing control. |
| [workbench_strip.cpp](../../src/adapters/qt/src/workbench_strip.cpp) | Retain toolbar, navigation, status/menu/icon controls and review-progress/custom caption variants. |
| [workbench_window.cpp](../../src/adapters/qt/src/workbench_window.cpp) | Review/Comments options and Reset already shared. Migrated Show later threads, composition-page Back/Restore, Home Copy/Cancel/Retry, Cancel setup, Open Project, Open Repositories, Copy message, existing-checkout choice, queue/notification Reset/Preview, account preparation, repository/server Cancel and tour Back. Chat factory migrates Context/New session/Continue old revision while retaining primary Send; finding factory migrates Open evidence/Ask follow-up/Add notes while retaining primary Convert and destructive Dismiss. Storage factory migrates Cancel maintenance while retaining Prune/Delete cache. Retain toolbar/navigation/icon controls, workshop Accept/Reject decisions, Apply classification, primary Add/setup/connect actions, destructive Remove, hyperlink guidance and standard confirmations. |
| [writing_hooks_panel.cpp](../../src/adapters/qt/src/writing_hooks_panel.cpp) | Factory migrates Restore default and Test saved hook. Retain Save instructions: explicit primary persistence action. |

## Surfaces added on main after the baseline

Main gained these button surfaces between the baseline and the 2026-10-22
rebase onto `4e318b625`:

| Translation unit | Disposition and controls |
|---|---|
| [about_window.cpp](../../src/adapters/qt/src/about_window.cpp) | Migrated Copy version info, a secondary action beside the window's own Close; its fixed width for "Copied" is measured from the shared size hint. Retain `link_button` products (the site and source links), which are hyperlinks, and the dialog button box's Close. |
| [commit_filter_view.cpp](../../src/adapters/qt/src/commit_filter_view.cpp) | Migrated the Commits tab's Clear all filters and its rematch offers, the file-tree chip's Rematch, Commits and Clear all filters, and the split-diff strip's per-commit diffs. The strip's diffs are exclusive choices, so the selected one has the chip and the others are bare words, as Overview's Open/All are. The flat actions stay bare at rest with the shared hover, press and focus treatment. |
| [generated_files_view.cpp](../../src/adapters/qt/src/generated_files_view.cpp) | Retain the group Toggle: an arrow disclosure with the folder row's ink and weight, which `quiet_tool_button` would draw natively anyway. |
| [workbench_window.cpp](../../src/adapters/qt/src/workbench_window.cpp) | Main's Compose review (BPR-1700) is a Review-bar toolbar action with its own lead and kept-draft marks, and the Review drawer's tabs (BPR-1684) are a tab bar; both retain their toolbar and navigation treatment. |

## Surfaces added by agent setup

[RFC-BPR-0092](../rfcs/RFC-BPR-0092-agent-first-run-setup.md)'s agent setup
([[BPR-PENDING-AQ6]]) adds these, built on the shared classes from the start:

| Translation unit | Disposition and controls |
|---|---|
| [agent_setup_view.cpp](../../src/adapters/qt/src/agent_setup_view.cpp) | Shared: Install Claude Code, Install Codex, the suggested clone folders, Choose a folder…, Copy (the sign-in command) and Stop. Retain each step's primary default action (Check and Check again, Use this clone, Choose its folder…), the agent and clone radio choices, and `link_push`: hyperlink-style Change, Look again, Agent settings, Use another program…, Use a private copy and Edit clone folders…, drawn in the palette's link colour. |
| [agent_setup_settings.cpp](../../src/adapters/qt/src/agent_setup_settings.cpp) | Shared: Set up agent…, Choose…, Remove (a clone folder, which deletes nothing on disk), Compare, Keep mine and Restore default. Retain Save (the programs and the review skill), Use version N (it replaces the edited copy, saved to Downloads first), and Forget (it removes a remembered link, as Forget server does). |
| [folder_list_editor.cpp](../../src/adapters/qt/src/folder_list_editor.cpp) | Retain Visual Studio's folder-list tools: icon-only New folder, Remove, Move down and Move up, and the editing row's icon-like **…** browse tool. |
| [agent_tab_view.cpp](../../src/adapters/qt/src/agent_tab_view.cpp) | Shared: Review this pull request, the one-click review beside the question field's Ask. Retain the model and effort control, a `menu_word` popup like History. The ready line's What the agent gets is a `disclosure_button`; its icon-only dismiss is retained. Check again, which takes `BannerFirst`'s place after an Ask that ended for a setup reason, keeps that primary treatment. |

## Dedicated specialized implementations

A broader scan against immutable baseline `74ef7f4fb` also finds these four
implementation units and the `discuss_header.hpp` class declaration. They
remain intentional variants, rather than untracked migrations:

- [destructive_question.cpp](../../src/adapters/qt/src/destructive_question.cpp):
  `question_button` and `danger_tool_button` distinguish confirmed destructive
  consequences and the safe default response.
- [disclosure_button.cpp](../../src/adapters/qt/src/disclosure_button.cpp):
  `disclosure_button` owns the title/chevron bar and its open state.
- [discuss_header.cpp](../../src/adapters/qt/src/discuss_header.cpp) and
  [declaration](../../src/adapters/qt/src/discuss_header.hpp): `discuss_tool`
  combines conversation glyphs, counts, filter chips, paired segments and the
  drill-down close mark in the existing Discuss header language (BPR-1337).
- [info_glyph.cpp](../../src/adapters/qt/src/info_glyph.cpp): `info_glyph` is the
  information icon (or one-character fallback) that opens a consequence popover.

The 2026-10-22 scan also counts `QAbstractButton` subclasses, which the
baseline scan did not. These are custom-painted conversation controls, not
native buttons, and keep their own presentation:

- [conversation_cards_view.cpp](../../src/adapters/qt/src/conversation_cards_view.cpp):
  `pull_request_chip` and `edge_chip`, the conversation list's chips.
- [conversation_open_card.hpp](../../src/adapters/qt/src/conversation_open_card.hpp):
  `post_tool` and `note_button`, the open card's post tools and primary note
  actions.
- [conversation_reactions.cpp](../../src/adapters/qt/src/conversation_reactions.cpp)
  and [declaration](../../src/adapters/qt/src/conversation_reactions.hpp):
  RFC-BPR-0091's `reaction_chip` and the picker's `reaction_option`, which
  show an emoji and its count as GitHub's reactions do.

The broad baseline scan has 51 files: the 46 table rows, these four `.cpp`
files and the one header. There is no unaccounted baseline file in that scan.
At `4e318b625` the scan of `new QPushButton`, `new QToolButton`, the two shared
classes and `QPushButton`/`QToolButton`/`QAbstractButton` subclasses finds 55
files, every one of them listed on this page.

## Completion and evidence

All 46 initial translation units and the surfaces main added are accounted for,
including every product of mixed factories. The two shared components and internal painter are the reusable
implementation, not new product surfaces. No deferred migration remains here.
Names above are source symbols or actual factory-result labels, so retained
variants can be checked directly against the linked construction and consumers.
The [verification ledger](../verification/RFC-BPR-0087.md) records the actual
focused tests, integration pass, full-window captures and platform limits.
Offscreen style renders never certify native desktop appearance.
