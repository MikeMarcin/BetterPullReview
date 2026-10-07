---
schema: "implement-rfc.card.v1"
id: "BPR-1089"
title: "Deliver resolution-aware Overview columns and exact drill-down"
status: "In Progress"
rfcs: ["RFC-BPR-0049", "RFC-BPR-0050"]
depends_on: ["BPR-1083", "BPR-1087"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-15","milestone":"overview","acceptance":["R20","R21","R23","R24","R27","R31","R33"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"},"sequence_after":["BPR-1088"],"coordinator_amendment":{"date":"2026-10-06","decision":"BPR-1088's landed settings (4a07f26bc) satisfy the dependency; its R20 owner look folds into BPR-1089's owner look"}}
acceptance: [{"criterion":"R20","outcome":"pending","revision":"b019915ee","evidence":{"kind":"observation","detail":"Folded from BPR-1088 by the coordinator's amendment. Settings' Overview chips now drive the Overview: application_overview_feedback_public_path ticks issue and nitpick in Reviews & queue > Comment labels and the composed Overview counts them (renderer.cpp issue 1 + Other 1, main.cpp nitpick 1). Settings' Open carries the round open mark (qt_feedback_label_settings). Pending: the owner look at Settings and the Overview (offscreen renders only; frontend-design skill not installed on this station)."}},{"criterion":"R21","outcome":"pass","revision":"b019915ee","evidence":{"kind":"command","path":"tests/application/overview_feedback_tests.cpp","detail":"No chips, issue+question and all twelve, Open and All: every file, folder, Not on a file and pull-request row's chips and Other drill into disjoint lists whose union is the included list, each list exactly its number; file rows plus Not on a file are the pull request, each conversation once (application_overview_feedback). core_feedback_partition: 2,000 generated populations x 10 scopes (directory and unattributed added) x both modes, every number's selection equals its count, dedicated + catch-all partition the included; merged file tallies equal the directory's. Qt and composed: chips issue 1, question 1, Other 1 on the same file; all twelve fold into +n with the menu reaching 13 entries."}},{"criterion":"R23","outcome":"pass","revision":"b019915ee","evidence":{"kind":"command","path":"tests/adapters_qt/pull_request_overview_page_tests.cpp","detail":"Open is the composed default (application_overview_feedback_public_path), follows Settings for each new pull request and never writes back; All shows each chip's breakdown: state bar, accessible 'issue 2 (1 open, 1 resolved), Other 2 (2 open) conversations'; composed after a website resolve: 'issue 1 (1 resolved), Other 1 (1 open)'."}},{"criterion":"R24","outcome":"pass","revision":"b019915ee","evidence":{"kind":"command","path":"tests/application/overview_feedback_tests.cpp","detail":"REST groups (unknown) give Open 0 with Uncertain 2 and resolution_unknown, never exact; an empty file under a partial collection is qualified. Qt: a cell with only an unknown-state issue shows the ? qualifier ('? 1'), the ? chip beside the switch counts unknown + not resolvable + unreadable once each with its limits in the tooltip; a blank cell is only a complete zero. Controls fail."}},{"criterion":"R27","outcome":"pass","revision":"b019915ee","evidence":{"kind":"command","path":"tests/end_to_end/setup_destination_public_path_tests.cpp","detail":"Composed: a click on renderer.cpp's issue chip lists exactly review-thread:PRRT_cc_1 in Discuss under 'issue · open · src/engine/renderer.cpp — 1 conversation' with focus on the list; Enter on the Comments cell opens the chip menu and Other lists exactly PRRT_cc_2; a website resolve + Refresh re-lists the same count as 0 without moving focus, and the Overview keeps its current cell. Sorting and drill-down read the same core projection (feedback_sort_key_of / select_feedback_units); folders sort by their own number."}},{"criterion":"R31","outcome":"pending","revision":"b019915ee","evidence":{"kind":"observation","detail":"Built: every chip draws through feedback_mark_theme (icons or letters, theme tints), four round state marks by shape, the Label key lists all twelve marks with names and shortcodes plus the state marks, accessible text per cell and per menu entry, anchored tooltips. Pending: owner look; offscreen renders only."}},{"criterion":"R33","outcome":"pending","revision":"b019915ee","evidence":{"kind":"observation","detail":"The surface works in the composed application offscreen (Linux, 1x, 1400x900) by pointer and keyboard. The native matrix (100/125/200%, themes, larger text, VoiceOver/Narrator, narrow/wide) is not run."}}]
---

# BPR-1089: Deliver resolution-aware Overview columns and exact drill-down

Pack alias: **PACK-CC-15** (live `BPR-1089`, allocated at import).

## Outcome and work

Implement the selected compact Overview headers, numeric rendering, catchall remainder, include-resolved behavior and uncertainty presentation. Sorting and activation use the same accepted conversation population. Preserve file reading/navigation and description geometry.

## Coordinator amendment (2026-10-06)

The coordinator ruled that [[BPR-1088]]'s landed settings (`4a07f26bc`: the
label helper, Overview counts and ordered label chips on the profile's
review behaviour) satisfy this card's dependency on it. BPR-1088 stays
parked for human review only for its R20 owner look, and that look folds
into this card's owner look. BPR-1088 moved from `depends_on` to
`metadata.sequence_after`, as [[BPR-1293]] records its sequence, because the
board keeps a card with an unfinished dependency in Backlog.

## Acceptance

- No-column, typical-subset and all-twelve states show a disjoint correct breakdown.
- Unknown resolution cannot look like a fully clear review, and all-state counts expose their breakdown.
- Keyboard/screen-reader activation opens exactly the counted conversations and retains focus across refresh.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Built and verified on Linux (bpr-main-linux, 2026-10-06)

Product commit `b019915ee`. Engineering owner `rfc-implementer`
(`claude-opus-5-5`, effort high). The frontend-design skill is not installed
on this station, so the surfaces were built from the owner's selected design
(BPR-1079 Overview B, frames v2) and the existing row language, with minimal
text, icons and tooltips, and checked from offscreen renders only.

**What was built.**

- **Core** (`feedback_partition`):
  - The projection carries each chip's state breakdown (`States`, and
    `CatchallStates`) and the ? count, `Uncertain`: units whose resolution is
    unknown or not resolvable, or whose label is unreadable, each once.
  - Two scopes: `directory` (a flat folder row's own files) and
    `unattributed` (Not on a file).
  - `merge_feedback_counter_tallies` sums disjoint tallies.
  - `select_feedback_units` lists the exact conversations behind any number
    (included, a chip, Other, ?) with the scope, chips and mode the
    projection used.
  - `feedback_sort_key_of` and `feedback_sorts_before` sort by number, a
    qualified count first at a tie.
- **Application**:
  - `overview_feedback.hpp/.cpp`: `project_overview_feedback` answers the
    Overview's request (Settings' chips, the view's mode and sort) from
    BPR-1087's kept tallies. A folder row is its listed files. Not on a file
    also takes any conversation on a file the pull request no longer lists,
    so the rows count each conversation once. `list_overview_drill_down`
    lists one row's number.
  - `feedback_counter_model` keeps the unattributed scope (re-tallied only
    when an unattributed conversation changes) and answers any scope with
    `tally_of`.
- **Composition** (`github_application.cpp`): the window observes each
  accepted snapshot into a `feedback_counter_model` before the overview is
  published. It answers the Overview's request, lists a count's
  conversations in Discuss, and lists the same count again after a refresh.
  No provider request; pins resealed.
- **Qt**:
  - The Comments column (216 px) draws chips through `feedback_mark_theme`.
    In Open the count is attention amber, outlined on folder and Not on a
    file rows. In All it is neutral over a state bar.
  - Other is a speech bubble with dots, Comments a plain bubble. "+n" folds
    the chips that do not fit, and a round qualifier sits at the right edge.
  - A "Not on a file" last row, and the Comments ▾ sort menu.
  - Beside the Files heading: Open (round open mark) and All, the ? chip,
    and the Label key (Alt+K).
  - The four round state marks are in `feedback_label_marks`, and Settings'
    Open shows the open one.
  - Discuss lists a drill-down exactly (`discuss_filters::Listed`), under a
    caption with a × to show every conversation again.

**Keyboard route (recorded in the delivery contract).** Left and Right move
along a file row's cells. Enter on the Comments cell opens a menu of every
chip, Other, the chips folded into "+n" and ?, with Sort by. The
context-menu key and a right-click open it on any row. Enter elsewhere still
opens the file. A click on a chip activates it and opens no file. Focus
returns to the same cell after Discuss.

### Acceptance

- **No-chip, typical-subset and all-twelve states each show a disjoint,
  correct breakdown; the catch-all remainder counts once.** Pass at
  `b019915ee`:
  - `application_overview_feedback` covers every row and number, in three
    chip states and both modes.
  - `core_feedback_partition` adds the overview numbers, 2,000 generated
    populations over ten scopes and the merge identity.
  - `qt_pull_request_overview_page` checks issue 1, question 1 and Other 1
    on one file, and all twelve folded into "+n" with 13 menu entries.
  - Composed: renderer.cpp shows issue 1 and Other 1 after Settings ticks
    issue and nitpick.
- **Unknown resolution never looks like a fully clear review; All shows its
  breakdown.** Pass:
  - REST-only conversations give Open 0 with `Uncertain` 2 and
    `resolution_unknown`. An empty file under partial collection is
    qualified.
  - A cell holding only an unknown-state issue shows "? 1". The ? chip
    names its limits.
  - All's accessible text reads "issue 2 (1 open, 1 resolved), Other 2 (2
    open) conversations".
- **Sorting and activation use the same accepted conversation population.**
  Pass. Both read the same core projection and selection. Sorting is folders
  by their own number, then their files, numeric, qualified first at a tie.
  The composed drill-down lists exactly the counted thread.
- **Keyboard and screen-reader activation opens exactly the counted
  conversations and keeps focus across a refresh.** Pass, composed:
  - Enter opens the menu, and Other lists exactly `PRRT_cc_2`.
  - After a website resolve and Refresh, the listed issue count becomes 0
    with focus kept on Discuss, and the Overview keeps its current cell.
  - Qt: a refresh and a sort keep the current Comments cell with no reset.
  - Every cell and menu entry has accessible text naming label, count, unit,
    mode and limits.
- **File reading, navigation and description geometry preserved.** Pass.
  Enter on other cells opens the file, and a chip opens none. The
  description's geometry is unchanged by chips and modes. The nested folders
  course passes.
- **Owner look: OPEN.** It covers this card's Overview surfaces and
  BPR-1088's R20 Settings look (coordinator amendment). R31 and R33 are
  pending that look; R33's native matrix is not run.

### Failing-before controls

Each mutation was applied, built, run and reverted.

| Mutation | Fails at |
|---|---|
| Directory scope includes subfolders (core) | "a directory holds its own files only; the root directory holds README.md" |
| ? drill-down leaves out not resolvable (core) | "generated selection is exactly the count" |
| ? count leaves out not resolvable (core) | "the pull request comment is not on a file, not open, and counted by ?" |
| Other's drill-down keeps dedicated conversations (core) | "Other lists the nitpick, the plain and the unlabelled conversations" |
| No qualified-first tie-break (core) | "at a tie the qualified count sorts first; equal keys keep the caller's order" |
| A chip's breakdown left out (core) | "All keeps each chip's state breakdown" |
| Unlisted files not in the remainder (application) | "the file rows and Not on a file together are the pull request, each conversation once" |
| Drill-down ignores the request's mode (application) | "a drill-down lists exactly its number's conversations, at the accepted epoch and mode" |
| Sort ignores folders (application) | "sorted by all open conversations: folders by their own number, then their files" |
| Remainder not re-tallied (application) | "round 0: the update names exactly the changed and removed scopes" |
| Other chip dropped with chips chosen (Qt) | "issue 1, question 1 and Other 1 (the plain thread)…" |
| No qualifier on a zero count (Qt) | "zero open beside one of unknown state shows ?, never a blank clean zero" |
| A label chip's click asks for Other (Qt) | "a click on the issue chip asks for exactly that file's issue count" |
| Enter on a Comments cell not intercepted (Qt) | "Enter on a Comments cell opens the menu of its chips" |
| A stale answer is taken (Qt) | "an answer to an earlier request is dropped" |
| A refresh rebuilds the rows (Qt) | "a refresh keeps the current Comments cell and resets nothing" |
| The listing leaves Hide resolved on (Qt) | "the whole pull request is in scope and no other filter narrows the list" |
| The cards ignore the listing (Qt) | "Discuss lists exactly the two counted conversations under the count's caption" |
| Settings' Open has no state mark (Qt) | "Overview counts: Open shows the round open mark beside its word" |
| A refresh leaves a listed count as it was (composition) | "after the resolve, the open issue count lists nothing, and says so" |
| The composition keeps the first snapshot's buckets (composition) | the same |

Three test gaps were found by controls and closed before commit: the folder
sort (a folder that outranks a single larger file), a zero-count qualifier,
and Settings' Open mark (it was set only on a behaviour publication; it is now
set when the row is built).

### Regressions and gates

Linux x86_64 (WSL2), GCC 15, Debug `-Werror`, `build/dev` whole tree:

- 43 focused rows (`core_feedback_*`, review-submission, feedback-counter,
  conversation, overview, settings, workbench-window and composed-github
  scales, label rows, nested folders, setup destination, review submission
  composition, label authoring, overview since review, the round-trip
  contract, `test_network_isolation_manifest`) and the new rows.
- `architecture_boundaries` and `architecture_boundary_contract` (220 s);
  `core-only` 11 of 11 (`core_feedback_*`, `core_dependency_boundary`,
  `core_boundary_contract`).
- `coding-format-check` (`/usr/bin/clang-format` 22.1.8), `git diff
  --check`, `DOCUMENTATION_OK`. `tidy.py` (clang-tidy) reports nothing in
  `feedback_partition.cpp`, `overview_feedback.cpp`,
  `feedback_counters.cpp`, `feedback_label_marks.cpp`,
  `github_conversation_panel.cpp`, `pull_request_overview_page.cpp` and the
  application test.

**Optimized tree** (`app-release`, `BUILD_TESTING=ON`, `-O3 -Werror`, machine
idle, `ctest -j1`, then deleted):

- Every target of this card built. One unrelated target fails at `-O3`:
  `tests/sqlite_contract/tour_owner_channel_tests.cpp` (`test_tour_owner_channel`,
  last changed by BPR-1227 at `03deabbe7`), with GCC 15's
  `maybe-uninitialized` on a `review_tour_source_material` designated
  initializer, the pattern `tests/AGENTS.md` names. It is not touched here.
- `application_large_review_responsiveness`, `_launch`, `_benchmark`,
  `_timing_control` and `application_first_tab_comparison` pass. No input
  budget failure; Linux reports, Mac enforces.
- New phases, 515 files and 1,000 threads, against the 100 ms target:
  `overview_counts_all` 2.7 ms, `overview_counts_open` 2.4 ms,
  `overview_sort_by_comments` 2.8 ms, `overview_file_order` 0.8 ms.
- `paged_review_and_git_load` (which now includes the counter model's first
  observation) has its largest input gap at 33 to 62 ms.
- The focused rows above pass in the optimized tree too.

**Pins.** The full-application and production-manifest pins were resealed
over `03deabbe7`, with no intervening pinned-scope commit (the dated "BPR-1089
reseal" comment). The mutation-branch pin did not move (`e067befe…a5df`).

### Limits

- **Owner look** open; offscreen renders at 1x only. R33's native matrix is
  not run. macOS and Windows runs are owed.
- **An accepted handoff is needed**, as for the review preflight: the
  counter model is keyed by it, so a window without one keeps the earlier
  thread counts in the Comments column and shows no chips.
- **A conversation on a file the pull request no longer lists** is counted in
  Not on a file, since no row exists for its file.
- **Coverage** is still BPR-1087's available or unavailable read; "stale" is
  not yet reported by the composition after a failed refresh.
- **Live GitHub** not used.
