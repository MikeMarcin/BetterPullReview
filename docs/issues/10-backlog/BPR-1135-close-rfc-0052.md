---
schema: "implement-rfc.card.v1"
id: "BPR-1135"
title: "Verify and close RFC-BPR-0052"
status: "Backlog"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1129","BPR-1130","BPR-1131","BPR-1132","BPR-1133","BPR-1134"]
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
closeout_for: ["RFC-BPR-0052"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0052

## Objective

Reconcile [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
against its linked implementation cards, direct evidence and the owner's
attended look at the built chrome on macOS and Windows.

RFC base: `0453103ddd11337ee8b3adf5053b25149104acd8`.
Requested RFC set: RFC-BPR-0052; workstream position: 1 of 1.
Planned cards: BPR-1129, BPR-1130, BPR-1131, BPR-1132, BPR-1133, BPR-1134, BPR-1135. Add subsequently allocated required slices to
this list and dependencies before closeout. Registration is not implementation.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01 The chrome above the comparison is the title row, then the review bar, then the Updates line's timeline while it is open, with no other row between them in any state, and the comparison's header is the document tab strip. The Updates line folds from the review bar without moving it, a new window starts with it folded, and each window restores its own choice. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-02 The title row holds Home, the drawer toggles, the pull request's name, the modality at its right beside refresh, the comments toggle carrying the new-comment count, account and settings, with the platform's window controls. No navigation, progress or composition control sits in it, and every control stays top level at each platform's width floor. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-03 The review bar reads, from its left, the Updates disclosure with the comparison and its dates, open or folded; then previous and next change with the position, previous and next file with the position, and private progress as an icon whose tooltip names the active tab's file state; and at its right Compose review and Comment as icons whose tooltips name the command and its key. Open, the Updates line's timeline runs the full width just below it. Compose review marks a kept review draft, and Review details is reached from the comparison's menu. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-04 The Updates line carries no permanent caption. Its tooltip carries the history statement, and the line shows a mark whenever history is incomplete or ambiguous, as RFC-BPR-0029 requires. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-05 Document tabs follow RFC-BPR-0025's tab contract: a tab is a file, a comparison and a reading position; clicking a file reuses the active tab, Cmd-click (Ctrl-click) and Open in New Tab open another, Duplicate Tab copies the exact view, previous and next file change the active tab, closing a tab follows the neighbour rule, and each window keeps its pull request's tabs across restart. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-06 Each tab has its own comparison. New tabs take the window's comparison, a tab with its own pair says so on its label, and the tab's menu returns it to the window's comparison or makes its pair the one new tabs take. The Updates line shows the active tab's pair, Overview and Discuss show the window's, late work cannot reach another tab, and no exact view is silently retargeted. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-07 A tab over an older pair places only the threads that belong to its revisions, says how many are on later updates with a way to show them, and offers no new comment off the current head. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-08 Comment starts a comment on the current line, or a reply to the selected thread, and its menu names the target; its arrow is where RFC-BPR-0048's labels are offered. Every thread card offers Reply, beside the code as in Discuss. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-09 The right tray's header holds tabs, Comments and Findings, and Tour once RFC-BPR-0039 provides it, with each tab's tools beneath the tabs rather than stacked in one column. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-10 Option-R (Alt+R) composes the review, Option-C (Alt+C) comments or replies, Ctrl+Tab and Ctrl+Shift+Tab move between tabs, Cmd-W (Ctrl+W) closes the tab, with the window moving to Cmd-Shift-W (Ctrl+Shift+W) if the tab takes its key, Esc returns from a composition page, and Option-] and Option-[ are reserved for RFC-BPR-0040's tour stops. Both composition commands are in the Pull Request menu, and no key sequence is used twice in the assembled window. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-11 The rows are measured on every supported platform and the tests follow them: application_uiforetw_selected_diff_contract and its course script take the measured stacks with the Updates line open and folded, BPR-0940's window course asserts the new arrangement, and each platform's width floor is swept again. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |

## Completion rules

Record the exact commands, results and producing commits for each criterion.
Every required linked card must be done, including slices allocated later.
Use focused changed-surface checks and a composed public path. Limit native
and attended claims to actual evidence. Run the documentation gate, then
reconcile the RFC's index status only when canonical order and its
dependencies allow.

## Milestone checkpoints

### m1-title-row-bar-tab-model-tray: complete on macOS, 2026-09-25 (main, `7f36927a`)

The owner activated RFC-BPR-0052 on 2026-09-25 ahead of canonical order. The
plan is [milestone-plan.json](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json)
([MILESTONES.md](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/MILESTONES.md)).

| Card | Product commit | Finish | Record |
| --- | --- | --- | --- |
| [[BPR-1131]] document tabs in the core and the host | `e94e67f1` | `5b18a7d5` | Done on macOS |
| [[BPR-1129]] the title row and the review bar | `96a86433` | `1e8f0206` | Done on macOS |
| [[BPR-1134]] the right tray's tabs | `2770ebb1` | `7f36927a` | Done on macOS |

m2's cards ([[BPR-1130]], [[BPR-1132]], [[BPR-1133]]) moved to Ready in
`95a44560`.

- **Changed paths.** Core: `document_tabs.{hpp,cpp}` (new),
  `window_review_restoration.{hpp,cpp}`, `review_submission`'s `kept`.
  Application: `document_tab_host.{hpp,cpp}` (new), the window restoration
  host. SQLite: the restoration store's version four. Qt: `workbench_window`,
  `workbench_strip`, `epoch_line`, `epoch_comparison_control`,
  `review_submission_editor`. App: the UIforETW capture in
  `github_application.cpp` (test build only). Tests: the new core and host
  suites, the restoration suites, the Qt window, title-row, focus-order,
  composed and epoch-comparison courses,
  `application_uiforetw_selected_diff_contract.cmake`, and
  `application_review_round_trip_composition_contract.cmake` (resealed by
  [[BPR-1129]], with its record). Documentation: the workbench specification's
  layout, primary actions and conversation drawer, and the system
  architecture's tabs.
- **Integration pass at `7f36927a`** (macOS 26.6.2 arm64, Qt 6.11.2,
  `build/dev` Debug): 38 of 38 pass in 250.4 s, including
  `application_composition_contract`. The `core-only` preset builds, and its
  163 rows pass.

  ```sh
  ctest --test-dir build/dev -j8 -R "^(application_composition_contract|core_document_tabs|application_document_tab_host|core_window_review_restoration|application_window_review_restoration|sqlite_window_review_restoration_storage|sqlite_private_store_open|application_uiforetw_selected_diff_contract|application_review_round_trip_composition_contract|application_github_public_path_uiforetw_74|qt_workbench_title_row_scale_(100|125|200)|qt_workbench_window_scale_(100|125|200)|qt_workbench_focus_order_scale_(100|125|200)|qt_composed_github_scale_(100|125|200)|qt_epoch_comparison_scale_(100|125|200)|qt_review_submission_editor_scale_(100|125|200)|core_review_submission|qt_workbench_conversation_reveal|qt_github_conversation_panel|qt_conversation_cards_view|application_agent_finding_composition_contract|application_agent_invocation_composition_contract|application_workbench_composition_contract|test_network_isolation_manifest|architecture_boundaries|core_dependency_boundary)$"
  ```

- **Not this milestone's failure.** `application_agent_finding_dif_context_contract`
  fails as [[BPR-0966]] records, unchanged by these cards.
- **Owed before this card can close** (none blocks m2):
  1. The Windows and Linux station runs, with the exact commands in each
     card's "Owed elsewhere" section. [[BPR-1129]]'s includes the Linux
     height measurement (834 is derived; the measured value becomes the floor)
     and the Windows width sweep to 902 and height at the owner's 850. Windows
     had about 857 with the entry row, so the 28-pixel bar may take it below
     850. If it does, the owner decides; the floor is not lowered here.
  2. The owner's attended look, with the questions recorded on [[BPR-1129]]
     (four) and [[BPR-1134]] (two). No session was prepared.
- **Next action.** A fresh implementer takes m2: [[BPR-1130]] (the fold, the
  caption, the readiness line's words, which it asks the owner about, and the
  floors re-taken open and folded), [[BPR-1132]] (the tab strip, composing
  [[BPR-1131]]'s host over the epoch and workbench hosts), and [[BPR-1133]]
  (Comment's arrow and target, and Reply on every thread card).

### m2-fold-tab-strip-comment-reply: two of three cards done on macOS; stopped at the owner's wind-down, 2026-09-25

| Card | State | Product commit | Finish | Record |
| --- | --- | --- | --- | --- |
| [[BPR-1130]] the Updates line folds and loses its permanent caption | Done on macOS | `fb4b8364` | `7444aeb8` | On the card |
| [[BPR-1133]] Comment names its target, and every thread card replies | Done on macOS | `7c99a1eb` | `6db29f76` | On the card |
| [[BPR-1132]] the document tab strip in the comparison's header | In Progress (claimed `9d1b17a4`) | none on main | — | Checkpoint on the card |

- **Done.** [[BPR-1130]]: the Updates disclosure folds the timeline from the
  review bar and by keyboard (the View menu's Updates Line, Space on the
  disclosure; no new key); the fold is core state kept in each window's own
  layout record (settings store version ten), a new window starts folded, and
  nothing folds on its own; the line is 34 pixels with no caption, its statement
  is its tooltip and description, and incomplete or ambiguous history is a mark
  on the line and on the disclosure. [[BPR-0795]]'s readiness row is unchanged
  pending the owner's answer. [[BPR-1133]]: Comment answers the selected thread
  or comments on the current line and names the target in its tooltip, the Pull
  Request menu and its arrow's menu (the plain comment alone until
  RFC-BPR-0048); every card beside the code offers Reply through the Discuss
  path; nothing publishes without Publish. Both have negative controls, renders
  at 100, 125 and 200 percent, and their records on the cards.
- **Work in progress.** [[BPR-1132]] is a patch, not on main:
  the historical saved patch (consumed by the resumed product commit below)
  (`BPR-1132-document-tab-strip.patch` against `c0fdd70a`, SHA-256
  `830375e7239d9328585f0ba6dbbb3758200fa26288f6cb6c76fc37ba3f53eb0c`). It
  builds and passes the courses the README lists; the two
  `github_application.cpp` pins fail until resealed, and its Qt tests, composed
  course, format, tidy, specification and record are still to write.
- **Measured floors (macOS 26.6.2 arm64, 1280x1000, offscreen, all four
  appearance profiles).** On main at `fb4b8364`: 879 folded and 845 open (the
  menu bar 20, title row 30, review bar 28, diff status 15, 6 of spacing, status
  bar 22; open adds the timeline at 78+34), asserted by the capture and its
  course script. On the [[BPR-1132]] patch: 872 folded and 838 open (the
  28-pixel tab strip in place of the status row and its spacing). Linux is
  derived from [[BPR-1036]]'s stack (884 and 850 on main, 876 and 842 on the
  patch); Windows keeps the owner's 850 for the open line (884 folded) and is
  unmeasured. If Windows measures below 850, that is the owner's decision, and
  no floor is lowered here.
- **Owner asks (no session prepared; one question per criterion).** On
  [[BPR-1129]] (four; its fourth, the comparison 6 pixels shorter, is answered
  by the measurement: 879 folded and 845 open against 829), [[BPR-1134]] (two),
  [[BPR-1130]] (four: where the readiness words go, which the card must ask
  before [[BPR-0795]]'s row changes; the fold; the incomplete-history mark; the
  line's "pair ready" words now in the comparison menu's tooltip),
  [[BPR-1133]] (three: replying on a thread's line; the arrow's menu until
  RFC-BPR-0048; Reply on the cards beside the code), and two prepared on
  [[BPR-1132]]'s checkpoint for when it is done ("+" duplicating the tab; the
  older-pair notice above unplaced later threads).
- **Owed runs (the stations are idle; nothing was run on them).** Windows
  (bpr-main-win) and Linux (bpr-main-linux), each with the exact command on its
  card: [[BPR-1129]], [[BPR-1131]], [[BPR-1134]] (m1), [[BPR-1130]] (with both
  heights read from `capture-facts.json`: `comparison_rect_in_window.height` and
  `updates_line_open_comparison_height`), and [[BPR-1133]].
- **Not this milestone's failure.** `application_agent_finding_dif_context_contract`
  as [[BPR-0966]] records.
- **Next step.** Apply the [[BPR-1132]] patch at the current main, finish it
  (its checkpoint lists what remains), then run m2's integration pass and
  record it here; m3 follows once m2's exits hold.

### m2 resume checkpoint — macOS engineering verified; human/native pending, 2026-09-25

This supersedes the earlier m2 wind-down and next-action list. Assignment
`rfc0052-20260925 / m2-resume`, mandate `owner-20260925-m2-resume`, amended to
prepare one fresh scoped attended session after macOS engineering. The owner
explicitly deferred Windows/Linux runs. No remote station or live GitHub review
mutation was performed.

- **Product:** `0994aa0ee80e542e980c5b5b8682e1aac0161ec0` completes [[BPR-1132]] from the saved patch,
  alongside already completed [[BPR-1130]] and [[BPR-1133]]. The source and
  workflow root is `/Users/jaeger/Documents/GitHub/BetterPullReview`, `main`;
  unrelated RFC pack import `fc4b39e8` is preserved. The saved patch and its
  README are removed after consumption; their SHA and original state remain
  in the historical card record.
- **Implementation:** document tabs and their path/pair labels, narrow overflow,
  unique keys, file gestures, older-pair notice and per-window exact restoration.
  Integration corrected request reuse, failure ownership, Home cancellation,
  queued modality/file precedence and reopening from the retained Files cursor.
  The full source review and both composition-pin reseals are in the product
  commit; the mutation-branch digest is unchanged.
- **Verification:** build, changed-file format and tidy pass. The 52-row macOS
  integration passed 50 rows and exposed two lifecycle defects. The affected
  11-row run passed 10, including the full profile restoration course (557.70 s),
  and exposed request failure ownership on tab return. Its correction passed
  tab switching/restart but the strengthened final click exposed the retained
  list bridge. The final 9-row run passes all rows (179.47 s), including the full
  tab/restart/reopen course (94.10 s), exact-single-request composition, all tab
  and window scales, and Files tree. Final architecture and clean tests-disabled
  Release/composition-pin verification also pass. The idle large-history rerun
  passes in 309.55 s; no follow-up failure card is needed. Documentation gate
  and all 64 documentation regression tests pass.
- **Measurements:** all four UIforETW appearance profiles retain 872 folded /
  838 open at 1280×1000; the document strip is 28 pixels. Linux 876/842 remains
  derived; Windows 884/850 remains unmeasured and its owner budget is unchanged.
- **Evidence:** command/result inventories and original failure/correction logs
  are in `/Users/jaeger/.codex/tmp/bpr-rfc0052-m2-yzl4rfoe`:
  `verification.json`, `correction-verification.json`, `gesture-verification.json`,
  `reopen-verification.json`, `pin-verification.json`, `integration-tests.json`
  and `evidence/`. They are local runtime artifacts, not repository content.
- **Pending:** m2 is not fully accepted. Exit 1 still needs the owner's readiness
  wording placement before [[BPR-0795]] changes; its row is unchanged. The 14
  recorded owner decisions, macOS VoiceOver, Windows native appearance/Narrator,
  Windows/Linux station courses and measured platform floors remain owed. A
  platform failure remains the originating implementation card's defect. m3 is
  not eligible until the m2 exits and remaining closeout acceptance hold.
- **Review preparation:** one fresh session under the runtime above will bind
  only those 14 decisions, one criterion per item with choices and Other/write-in,
  to a frozen app and static renders. Its final identity and command ownership
  are recorded in [the durable handoff](../../rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/HANDOFF-m2-2026-09-25.md).
- **Workflow:** MASK preflight is `ready` / `ready_pinned` after the coordinator
  removed disposable installed bytecode; route `rfc-implementer`, `gpt-6-astra`,
  `xhigh`, `host_load: not_verified`. Skill CLIs use
  `PYTHONDONTWRITEBYTECODE=1`. The parent-created selection is preserved in
  `strategy-selection.json`; the final obligation binding is `m2-checkpoint.json`.
