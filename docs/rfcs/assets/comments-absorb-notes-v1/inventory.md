# Inventory: where notes exist today

Read at `47a5ed625` on `claude/v0-5-0-prerelease-targets-52265d`. Paths are
relative to the repository root. "Verified" means the file was opened; "ran"
means the built application or a built test drew it offscreen into
`ref/` beside this file (no desktop was captured).

Four different things are called a note in this code base. Only the first two
are in the proposal's scope.

| Kind | What it is | In scope |
|---|---|---|
| **Private note** | A reviewer's private text on a pull request, a file or a line. The Notes tab. | Yes |
| **Local-draft note** | An author's or harness's text on a local PR draft (RFC-BPR-0108). | Vocabulary only; see Q8 |
| **Review Cycle note** | A cycle's feedback item (`I<n>`), frozen into rounds and shared. | No |
| **`note:` label** | The Conventional Comments label on a published comment. | No; it is a label, not a kind of record |

## 1. The model and its storage

| Item | Where | Notes |
|---|---|---|
| Note record | `src/application/include/bpr/application/review_ledger_port.hpp:65-104` (`local_note_id`, `local_note_revision`, `local_note_attachment`, `local_note_record`) | Text, optional file and comparison, optional line, the file's path when written. Revision zero is an unsaved candidate. |
| Note bounds | same file `:58` (`maximum_private_notes = 4096`), `:63` (`maximum_note_path_bytes = 4096`); `src/core/include/bpr/core/agent_context_supplement.hpp:35` (`maximum_note_bytes = 16 KiB`) | A draft body may be 64 KiB (`src/core/include/bpr/core/revisioned_review_draft.hpp:11`). |
| Note store calls | `review_ledger_port.hpp:279-286` (`store_note`, `erase_note`), `private_ledger_observation::Notes` `:154` | Compare-and-replace by revision. |
| Anchor and placement | `src/core/include/bpr/core/note_placement.hpp` (`note_line_anchor`, `note_anchor`, `note_placement_kind` = pull_request, other_file, whole_file, placed, outdated; `note_outdated_reason`; `place_note`; `note_line_anchor_at`) | "The note is never moved to a nearby line." A line holds in its own comparison and in any other whose side shows the same blob. |
| SQLite tables | `src/adapters/sqlite/src/private_review_database.hpp`: `private_legacy_note` (note id to private thread), `private_note_path` (schema 8), `private_note_line` (schema 10), `private_scope_state.note_revision` | A note is a one-turn private thread since RFC-BPR-0023. Ledger schema is 11 (`review_ledger_database.hpp:29`). |
| SQLite code | `src/adapters/sqlite/src/private_review_store.cpp:268,421-424,443-488,545-554,596-615,721-724,1496`; `review_ledger_database.cpp:196-212,300-318,1619-1622,1871-1875`; `review_ledger_store.cpp`; `observing_review_ledger_store.cpp` | Includes the schema 5 to 6 and 6 to 7 note migrations already shipped. |
| Draft record (the other half) | `src/core/include/bpr/core/review_round_trip.hpp:41-141` (`review_draft_state` = drafting, publishing, published, discarded; `review_pending_mark`; `review_draft_place`; `review_comment_draft`) | Anchor is a `source_line_ref` (comparison, side, line), optional range, optional reply thread, optional pending mark. No pull-request or whole-file anchor. |
| Draft standing | `src/core/include/bpr/core/draft_roster.hpp:28-39` (`draft_standing` = in_review, not_in_review, held, reply), `project_draft_roster`, `draft_counts_by_file`, `drafts_outside_review`, `plan_add_to_review` | BPR-2092 to BPR-2094. |
| Draft store | `private_draft` table; schema 9 "retains completed history separately from the 64 active-draft allowance" (`private_review_database.hpp`, `PrivateRetainedDraftState`) | 64 active drafts a review against 4,096 notes: a merge must not put notes under the draft allowance. |
| Private text into a draft | `revisioned_review_draft.hpp:69-73` (`promote_private_text`); `src/app/src/github_application/window_agent_chat.cpp:78-200` (`promote_private_item`, `promote_line_note`) | Today promotion **copies** the text and the note stays (RFC-BPR-0085, "Promoting private material"). |
| Shared counts | `src/core/include/bpr/core/feedback_partition.hpp:62-67` (`feedback_unit_origin` = published, private_note, draft, pending_publication), `:138` (`feedback_population_exclusions::Private`, `Unpublished`) | Notes and drafts are already excluded from label counters by origin. |

## 2. Surfaces that show or write private notes

| Surface | Where | Today |
|---|---|---|
| Notes tab of the right drawer | `src/adapters/qt/src/workbench_window_central_surface.cpp:1604-1627` (tabs Comments, Notes, Tour, Agent); `workbench_window_comments_and_threads.cpp:469-486` (tab tooltips) | Ran: `ref/notes-light/app-notes.png`. |
| The notes panel | `src/adapters/qt/include/bpr/qt/private_notes_panel.hpp`, `src/adapters/qt/src/private_notes_panel.cpp` (1,563 lines) | Dashed cards with a lock at their lines; edge chips for notes on the pull request, the whole file and other files; open card with Ask agent, Delete, Enhance, **Draft comment**, **Save**. Ran: `ref/notes-light/app-note-open.png`, `app-notes-elsewhere.png`. |
| Gutter mark of a noted line | `private_notes_panel.hpp:86-97` (`private_note_mark`), `workbench_window_comments_and_threads.cpp:597` | A dashed cell; separate from a thread's mark. |
| Key and menu | `src/adapters/qt/src/keyboard_shortcuts.cpp:135` (`command::new_note`, **N**, "A private note on the selected line", "In Notes"); `newLineNoteAction` | |
| Overview's drawer, Notes tab | `src/adapters/qt/src/overview_status_drawer.cpp:67,80,147,198,272` (`overview_drawer_tab::notes`); `private_notes_panel.hpp` `set_pull_request_only` | Pull-request notes only (BPR-1781). |
| Overview, My review | `src/adapters/qt/src/pull_request_overview_page.cpp:140,838`; `pull_request_overview_people_and_status.cpp:643-682`; `src/application/include/bpr/application/pull_request_overview.hpp:137-140` (`PrivateNotes`, `NotesReadiness`) | A "Private notes" count. Ran: `ref/overview/overview-drafts.png`. |
| Overview, Files table, **Note** column | `src/adapters/qt/include/bpr/qt/pull_request_overview_page.hpp:215` (`note_column`); `src/application/src/pull_request_overview.cpp:918-970` (`row.Note`) | The file's private note, first note per file, as text. |
| Overview, Comments column | `pull_request_overview_delegate_and_rows.cpp:473`, `pull_request_overview_file_model.cpp:233`, `pull_request_overview_support.cpp:436`, `pull_request_overview_chips_and_details.cpp:111` | Comment chips and one drafts chip (pencil, count, amber dot when any is not in the review). No note chip. BPR-2093. |
| Threads | `src/adapters/qt/src/conversation_finder.cpp:88-98,1275-1287,2053,2131-2143,2827-2831,3155` | Lists threads and my drafts ("Draft", "Draft reply", "Not in review"); filter chips Only new, Hide resolved, Label, **My drafts**. Notes are not listed. Ran: `ref/threads/threads-drafts-all.png`. BPR-2092. |
| Finish your review | `src/adapters/qt/src/review_submission_editor.cpp:1062,1602,1727` | "N drafts aren't in this review", Add all, Add, Leave Out of Review. Notes are not mentioned. Ran: `ref/drafts-light/app-sheet.png`. BPR-2094. |
| A draft's card in Comments | `src/adapters/qt/src/conversation_open_card.cpp:2004,2707-2740,2966-2995`; `conversation_cards_view.cpp:2739-2816,3633-3653` | "Draft" with a lock; "Private until you publish"; Delete, Add single comment, Start a review or Add review comment. A saved draft not in the review is already captioned **"Your note, not published"** (`conversation_cards_view.cpp:2801`). |
| Private history | `src/adapters/qt/src/workbench_window_github_presentation.cpp:1895-1899` ("Private notes", "Private history and notes"); `src/adapters/qt/src/private_review_panel.cpp:440` | RFC-BPR-0023's panel; hidden behind the Agent tab's history since BPR-1594. |
| Insights | `src/adapters/qt/src/insights_window.cpp:802,841`; `src/core/include/bpr/core/insights.hpp:110,125` (`metric::notes`) | A "Private notes" tile. |
| Settings and maintenance wording | `src/adapters/qt/src/profile_data_reset_views.cpp:121,136,149,159,290,390`; `release_maintenance_panel.cpp:422`; `workbench_window_header_and_queue_settings.cpp:1139`; `src/core/include/bpr/core/profile_data_reset.hpp:67-71` | "Private notes and their threads", "Notes, drafts and read marks". Reset offers a copy of the store files; there is no note export. |
| Attended look | `src/app/main.cpp:274,6298-6626` (`--attended-notes-look`, `BPR_ATTENDED_NOTES_RENDER`) | Used above for the reference renders. |

## 3. Agent surfaces that offer a note

| Surface | Where | Today |
|---|---|---|
| Answer footer | `src/adapters/qt/src/agent_tab_view.cpp:1417-1420,1600-1601` | **Draft comment** and **Save as note** side by side; each becomes Go to draft or Go to note. RFC-BPR-0094 Q2 as amended 2026-10-27. |
| Finding card | `agent_tab_view.cpp:3315-3382`; `src/core/include/bpr/core/agent_finding_card_memory.hpp:40-67` (`noted`, `Note`) | **Draft comment**, **Save as note**, "Saved as a private note." |
| Draft all | `src/core/include/bpr/core/agent_draft_all.hpp:28-29,83` | A finding already saved as a note "may still be drafted". |
| Finding queue | `src/adapters/qt/src/finding_queue_view.cpp:122` ("Add to private notes"); `src/application/src/agent_finding_host.cpp:22-52,150` (`add_to_private_notes`); `agent_finding_delivery.cpp` | |
| Promotion policy | `src/core/include/bpr/core/agent_finding_promotion.hpp:26-36,68-77,181-232` (`agent_finding_promotion_choice` = draft_comment, defect, private_note) | The note is the default and the only choice for a finding whose anchor does not hold. |
| Explain this file | `src/adapters/qt/src/explain_file_card.cpp:72,146,195` (**Keep as note**, Go to note); `src/core/include/bpr/core/explain_file.hpp:84-90` (`explain_note_text`); `src/app/src/github_application/window_explain_file.cpp` | A file-level note with its agent origin. RFC-BPR-0117 AC03; card BPR-2076 (parked). |
| Ask agent about a note | `src/application/include/bpr/application/private_review_application_host.hpp` (`record_note_press`, beside `record_workshop_press` for a draft); `src/core/include/bpr/core/agent_chat_scope.hpp:36,187,538`; `agent_question.hpp:31`; `window_agent_setup.cpp:774` | The chip reads "Your note on src/cache.cpp line 42"; the prompt "Explain the code this note is on". RFC-BPR-0094 Q3; BPR-1773. |
| Private context opt-ins | `src/core/include/bpr/core/agent_context_supplement.hpp:108-115,511,577,630-690` (`supplement_note`, `agent_private_context_optin { Notes, Drafts }`); `src/adapters/qt/src/side_chat_panel.cpp:138-160,379`; `agent_launch_panel.cpp:152,179,418` | Two separate opt-ins by design: "a reviewer may well want the agent to see the comment they are drafting without also handing it the notes they took about the author." |
| Enhance | `src/adapters/qt/include/bpr/qt/enhance_control.hpp`; `private_notes_panel.hpp` (`set_enhance_dispatch`); `src/adapters/qt/src/writing_hooks_panel.cpp:56-61,221`; `src/core/include/bpr/core/review_preparation.hpp:17-23`; `src/application/resources/review-preparation/enhance-writing.md` | One prompt, `enhance.writing`, already covers draft comments, replies and notes (owner, 2026-11-10). BPR-2051. |
| Returning review | `src/core/include/bpr/core/returning_review.hpp:102-105` (`proposed_fix_facts::Note`) | "A private note: never asked 'was this addressed'." RFC-BPR-0116. |
| Tour agent context | `src/application/src/tour_agent_context.cpp:148` (`context_omissions`: `review_notes_not_selected`, `comment_drafts_not_selected`) | A published key in `bpr.tour-context.v1`. |
| MCP context helper | `src/adapters/mcp/include/bpr/adapters/mcp/mcp_context_helper.hpp:30-36,119` | "There is no write, no draft, no publish, and no note store." Nothing to change. |

## 4. Local PR drafts (author side)

| Item | Where | Today |
|---|---|---|
| Record | `src/core/include/bpr/core/local_draft.hpp:48-50,390-405,429-430,484-503` (`local_draft_note` = id, text, path, candidate, `note_origin` author or harness; 512 notes of 16 KiB) | Whole-change or path notes; no line, no state. |
| Notes area | `src/adapters/qt/src/local_draft_parts.cpp:269-287,346-367,504,689,750-765` ("Notes", "Add a note", "Remove Note") | RFC-BPR-0108 AC04. |
| CLI | `src/app/src/local_draft_cli.cpp:55` (`bpr draft open --context` "adds a note"); `src/app/src/local_draft_handoff.cpp:79` (JSON `"notes"` count) | A published CLI contract. |
| Findings become notes | RFC-BPR-0115 "Findings on a local draft become notes" (**Keep as note**, **Keep all as notes (N)**, Fix on a note, Check the fix; AC01, AC05, AC06); card BPR-2069 (backlog) | A local draft has no pull request, so no draft comment exists there. |
| Publication | `src/core/include/bpr/core/local_draft_publication.hpp:108-109` (`PrivateNotes`); RFC-BPR-0111 ("what stays private is the draft's notes and agent conversations"); RFC-BPR-0110 AC02 | |

## 5. Places checked that hold no private notes

- **Find and Find in files** (RFC-BPR-0077): `src/core/include/bpr/core/source_search.hpp` and `home_filter.hpp` name no note. Threads' "Find a thread" searches threads and my drafts only.
- **Discuss**: shows no note and no line draft ("Your draft is written beside its line, in Review", `conversation_finder.cpp:2276`).
- **`bpr tour` and `bpr cycle` CLIs**: `bpr cycle note` and "Share notes" are Review Cycle notes (`src/app/src/cycle_cli.cpp:302-461`, `cycle_commands.cpp`, `review_cycle_view.cpp`, `review_cycle_share_sheet.cpp`, `review_cycle_promote_sheet.cpp`); `tour_assist.cpp:258-265` `notes` are the tour service's own remarks. Neither reads a private note.
- **Agent and prompt sharing** (RFC-BPR-0107): carries prompts and agent setup, never note text. Its retired purpose ID `enhance.note` is read as `enhance.writing`.
- **Import**: nothing imports private notes.

## 6. Every place that offers both, or a note where a draft would do

1. Agent answer footer: Draft comment + Save as note.
2. Agent finding card: Draft comment + Save as note; Draft all's "noted".
3. Finding queue: Add to private notes.
4. Explain this file card: Keep as note.
5. What changed since my review (RFC-BPR-0116 mockups): Save as note.
6. Local draft finding card: Keep as note, Keep all as notes (N).
7. A note's own card: Save + Draft comment.
8. A draft's own card: Add single comment + Start a review, with no way to make it private again short of deleting it.
9. Toolbar and keys: Comment… and N.
10. Agent opt-ins: Include my private notes + drafts.
11. Overview: Private notes count, Note column and the drafts chip.
12. Threads: My drafts (drafts only).
13. Finish your review: drafts not in the review (drafts only).
14. Ask agent: on a thread and on a note, by two recording paths.
15. Enhance settings: "Comments, replies and notes".

## 7. Tests that pin today's behaviour

`tests/core/note_placement_tests.cpp`, `feedback_partition_tests.cpp`, `feedback_private_promotion_tests.cpp`, `agent_finding_promotion_tests.cpp`, `agent_context_supplement_tests.cpp`, `returning_review_tests.cpp`, `local_draft_publication_tests.cpp`; `tests/adapters_qt/private_notes_panel_tests.cpp`, `conversation_finder_tests.cpp` (`--render-bpr-2092`), `pull_request_overview_page_tests.cpp` (`--drafts-look`), `side_chat_panel_tests.cpp`, `agent_launch_panel_tests.cpp`, `finding_queue_view_tests.cpp`, `enhance_control_tests.cpp`, `drawer_parts_tests.cpp`, `workbench_window_tests.cpp`; `tests/sqlite_contract/private_review_storage_tests.cpp`, `private_review_host_tests.cpp`, `review_ledger_storage_tests.cpp`, `review_ledger_legacy_fixture.hpp`; `tests/application/review_ledger_host_tests.cpp`, `pull_request_overview_tests.cpp`, `feedback_counters_tests.cpp`, `mcp_context_helper_tests.cpp`; `tests/end_to_end/explain_file_public_path.cpp`, `scoped_writing_agent.cpp`, `review_submission_composition_public_path.cpp`.

## 8. RFCs and cards that say "note"

RFCs: 0007 (ledger, with the 2026-10-06 and 2026-10-17 note amendments), 0011, 0012, 0023, 0049, 0085 (D3, "Promoting private material", the tab list), 0086, 0094, 0101, 0102, 0107, 0108, 0110, 0111, 0113, 0115, 0116, 0117, 0118, 0121, 0122, 0123, 0125.

Cards: BPR-1210, BPR-1245, BPR-1345, BPR-1591, BPR-1592, BPR-1594, BPR-1603, BPR-1773, BPR-2051, BPR-2092, BPR-2093, BPR-2094 (all Done); BPR-2078 (in progress); BPR-2076 (parked); BPR-2069 (backlog).
