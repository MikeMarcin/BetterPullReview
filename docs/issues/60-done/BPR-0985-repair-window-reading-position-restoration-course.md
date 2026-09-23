---
schema: "implement-rfc.card.v1"
id: "BPR-0985"
title: "Repair deferred reading-position restoration so the window review restoration course passes"
status: "Done"
rfcs: ["RFC-BPR-0025"]
depends_on: []
priority: "P1"
metadata: {"found_by":"BPR-0597 RFC-0025 closeout and the merge of codex/rfc-0018-0026-closeout into main, 2026-09-22","found_at":"6a897af8","last_pass":"ce63c67b (BPR-0709)","related_rfcs":["RFC-BPR-0024"],"related_cards":["BPR-0597","BPR-0595","BPR-0709","BPR-0710","BPR-0795"],"closeout_card":"BPR-0597","milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c9c48800376f71be14e8351d64ba0970df878f4f","evidence":{"kind":"observation","detail":"auth-home: assertion superseded by owner-accepted BPR-0795 loading Overview (9c670d88; v004 decision 433), corrected at 2fbcc8cb and kept. auth-reading-gesture/unchanged/home, unavailable-semantic, auth-restore: product regression from cc038748 (RFC-0037 default conversation row re-revealed after each diff publication as a reading gesture, cancelling deferred restoration); backtrace from cancel_restoration, git log -L sole writer, and disabling that statement at 6f08923f made the reading phases pass. No restoration-course assertion changed.","path":"docs/issues/60-done/BPR-0985-repair-window-reading-position-restoration-course.md#ac1-what-changed-each-phase"}},{"criterion":"AC2","outcome":"pass","revision":"c9c48800376f71be14e8351d64ba0970df878f4f","evidence":{"kind":"command","detail":"Full restoration course passes at c9c48800 (570.40 s): gesture races (changed line, unchanged group, Home) win over deferred restoration; restart restores both windows' exact comparisons, files and semantic selections; unavailable anchor/file/semantic/content keep destination and sibling and report explicit failure. qt_workbench_conversation_reveal 11 courses pass 1.83 s.","path":"build/bpr0985-ev-restoration-course.json"}},{"criterion":"AC3","outcome":"pass","revision":"c9c48800376f71be14e8351d64ba0970df878f4f","evidence":{"kind":"command","detail":"application_profile_window_review_restoration_public_path passes every phase on main at c9c48800 (570.40 s) and on the identical candidate (578.68 s). Negative control (chosen_conversation returning the current index) fails auth-reading-gesture 'gesture precedes deferred reading restoration completion' and auth-restore 'restart preserves exact anchors, commit OIDs, file and complete semantic identity'; the new Qt course fails its named assertion.","path":"build/bpr0985-negative-control-course.log"}},{"criterion":"AC4","outcome":"pass","revision":"c9c48800376f71be14e8351d64ba0970df878f4f","evidence":{"kind":"command","detail":"core_window_review_restoration, application_window_review_restoration, sqlite_window_review_restoration_storage, restart 7.38 s, multi-window 77.38 s, layout 24.67 s pass on the c9c48800 content, with diff workbench path 12.15 s, ledger composition 35.72 s and setup destination 38.51 s. Neighbouring Qt suites pass; qt_composed_github/qt_workbench_window (BPR-0940) and application_threaded_reply_contract (40 s budget) stop identically before the repair.","path":"build/bpr0985-focused-public.log"}}]
---

# BPR-0985: Repair deferred reading-position restoration so the window review restoration course passes

## Observation

`application_profile_window_review_restoration_public_path` (the
[[BPR-0709]]/[[BPR-0710]] course) last passed at `ce63c67b` on 2026-09-08. It
is red on every head examined since:

| Head | First failing phase | Message |
|---|---|---|
| main before the merge, `5b14d7e8` | `auth-home` | pending restoration leaves Home available for user navigation |
| branch tip `317f0023` and merge `6a897af8` | `auth-reading-gesture` | gesture precedes deferred reading restoration completion |

`2fbcc8cb` ([[BPR-0597]]) corrected the `auth-home` assertion to the
owner-accepted [[BPR-0795]] behavior (Overview's loading state shows and Home
stays one gesture away), which is why the merged head gets further. BPR-0597's
diagnostic driver ran every phase past failures at `2fbcc8cb` and found:

- `auth-reading-gesture`, `auth-reading-unchanged`, `auth-reading-home`:
  "gesture precedes deferred reading restoration completion". The course
  queues a gesture from the right pane's `contentsChanged` and expects the
  window's Home notice to still read `Resuming…`
  (`github_application.cpp` `restoration_notice`).
- `unavailable-semantic`: "unavailable reading reports explicit failure"; the
  destination and sibling reading are kept, the explicit report is missing.
- `auth-restore`: "restart preserves exact anchors, commit OIDs, file and
  complete semantic identity". Comparison, both anchors/OIDs and the file
  restore exactly; only the semantic selection differs. `auth-close` and
  `auth-verify` depend on it and were not reached.

These are RFC-BPR-0024 restoration behaviors. They are the blocker recorded on
BPR-0597 for RFC-BPR-0025 AC-05 (reading position across restart) and AC-06
(explicit unavailable-reading report), and they bear on RFC-BPR-0024 AC-07,
whose closeout on [[BPR-0595]] relied on retained BPR-0709/0710 evidence.

## Acceptance

- AC1: Each failing phase is traced to the change that altered it and
  classified as a product regression or an assertion superseded by an
  owner-accepted behavior. An assertion changes only in the second case, with
  the accepting decision named.
- AC2: A gesture made before deferred reading restoration completes wins over
  it; a restart restores the exact comparison, file and semantic selection; an
  unavailable reading keeps its destination and reports an explicit failure
  without retargeting.
- AC3: `application_profile_window_review_restoration_public_path` passes every
  phase on main, and a discriminating negative control fails the phase it
  names.
- AC4: `core_window_review_restoration`, `application_window_review_restoration`,
  `sqlite_window_review_restoration_storage`, and the restart, multi-window and
  layout public paths still pass.

## Boundaries

Fixture-only; no live provider, credential or GitHub mutation. RFC-BPR-0024's
status is not changed by this card; its closeout owner records the result on
BPR-0595.

## Repair — 2026-09-22

Run `main-closeout-20260922`, milestone `close-rfc-0025` of plan
`ux-redesign-pack-0028` (amended at `ff4ba51b` to add this card), sole
non-delegating `rfc-implementer` on `main` under the coordinator-implementer
recipe. Claimed at `bb341002`; repair `c9c48800`.

### AC1: what changed each phase

| Phase | Changed by | Classification |
|---|---|---|
| `auth-home` | `9c670d88` ([[BPR-0795]]): an opening read enters Overview's loading state at once instead of keeping Home | Assertion superseded by owner-accepted behaviour: the owner approved the prompt transition, loading animation and status in v004 decision 433. Corrected at `2fbcc8cb` ([[BPR-0597]]); kept unchanged here |
| `auth-reading-gesture`, `auth-reading-unchanged`, `auth-reading-home` | `cc038748` (RFC-BPR-0037 "integrate first-PR reading surfaces") | Product regression, repaired |
| `unavailable-semantic` | `cc038748` | Product regression, repaired |
| `auth-restore` (and the unreached `auth-close`, `auth-verify`) | `cc038748` | Product regression, repaired |

`cc038748` made the conversations drawer set its first thread current when
nothing was, so its detail has something to show; its own Qt test says that
index "initially selects a detail without moving code". But the workbench
re-synchronised the drawer's current thread with the code after every
published file diff (`set_conversation_endpoint_placement`) and after every
conversation adoption. That revealed the default thread's line and, through
`diff_workbench::select_line`, reported it as a reading gesture. A temporary
backtrace in `cancel_restoration` showed the call on the restored file's first
publication: `diff publication → set_conversation_endpoint_placement →
synchronize_selected_conversation → reveal_conversation_line → select_line →
emit_reading_gesture → cancel_restoration(reading gesture)`. So the deferred
restoration was cancelled before it ran: the gesture races found the notice
already cleared, `auth-restore` kept the thread's line instead of the saved
semantic selection (comparison and file were already exact), and
`unavailable-semantic` never reached the validation that reports the explicit
failure.

`git log -L` names `cc038748` as the only writer of the default-row statement.
Disabling that one statement at `6f08923f` (a local experiment, not committed)
made `auth-reading-gesture`, `auth-reading-unchanged`, `auth-reading-home` and
`unavailable-semantic` pass. `cc038748` (2026-09-19) is older than
[[BPR-0595]]'s closeout (`eb73df8b`, 2026-09-22), so the reading phases were
already broken when RFC-BPR-0024 closed; from `9c670d88` on, `auth-home`
failed first and hid them. No assertion in the restoration course was changed
by this card.

### The repair (`c9c48800`)

The drawer now records when its current row is its own default
(`github_conversation_panel::current_is_default`). Re-synchronising after a
publication or an adoption reveals only a thread the reviewer chose
(`workbench_window::chosen_conversation`). The card, thread-map, Reply and
Open in code gestures go through `github_conversation_panel::choose`, so
choosing the thread that is current only by default is still announced and
still moves code. The drawer's presentation is unchanged. The workbench
specification's conversations section says so.

One Qt course assertion was reached through the old behaviour: the refresh
course in `test_conversation_selection_reveals_exact_diff_anchor` set the
current index to its sole thread, which was already the default row and so
emitted nothing; its anchor came from the default row's reveal. It now chooses
that thread. The assertion itself ("the refresh fixture begins with its
current right-side line anchored") is unchanged. The basis is the design
`cc038748` states for the default row, not a new owner decision.

### Verification

- **AC2, AC3.** `application_profile_window_review_restoration_public_path`
  passes every phase: 578.68 s on the uncommitted candidate
  (`build/bpr0985-restoration-course-c1.log`), then 570.40 s at `c9c48800`
  (`build/bpr0985-ev-restoration-course.json`). The phases cover a gesture
  winning over deferred restoration (changed line, unchanged group, Home), the
  exact restart restoration of both windows' comparisons, files and semantic
  selections, and the unavailable anchor, file, semantic and content readings,
  each keeping its destination and sibling and reporting an explicit failure.
- **Negative controls.** With `chosen_conversation` returning the current
  index again, `auth-reading-gesture` fails "gesture precedes deferred reading
  restoration completion" and `auth-restore` fails "restart preserves exact
  anchors, commit OIDs, file and complete semantic identity"
  (`build/bpr0985-negative-control-course.log`), and the new Qt course fails
  "placing the endpoints again after a published diff leaves a default
  thread's line unrevealed and reports no reading gesture"
  (`build/bpr0985-negative-control-qt.log`).
- **New selector.** `qt_workbench_conversation_reveal` runs the window's
  eleven conversation courses directly and passes 1.83 s at `c9c48800`
  (`build/bpr0985-ev-conversation-reveal.json`). They include the new
  `test_a_default_conversation_row_never_moves_code`. The full
  `qt_workbench_window_scale_*` run stops earlier at [[BPR-0940]]'s epoch-row
  assertion. With that course and
  `test_review_history_state_is_explicit_in_workbench_text` skipped by a local
  diagnostic edit, the rest of the window suite passes at `bb341002` and on
  the candidate (`build/bpr0985-window-skip940-{baseline,fix}.log`). The
  history-text course fails identically at `bb341002`, before the repair
  (`build/bpr0985-history-course-baseline-bb341002.log`). It lies past
  BPR-0940's stop in the window-scale registrations that card reconciles.
- **AC4** on the candidate content of `c9c48800`
  (`build/bpr0985-focused-public.log`): `core_window_review_restoration`,
  `application_window_review_restoration`,
  `sqlite_window_review_restoration_storage`, restart 7.38 s, multi-window
  77.38 s, layout 24.67 s, plus `application_diff_workbench_public_path`
  12.15 s, `application_review_ledger_composition_contract` 35.72 s and
  `application_setup_destination_public_path` 38.51 s (RFC-BPR-0037's
  combined Discuss/Review course).
- **Neighbouring Qt suites** (`build/bpr0985-focused-qt.log`): conversation
  panel, cards, thread map, diff workbench, review submission editor, composed
  markdown, public-preview conversation, resident drawer filters, focus order
  and the review-submission composition path pass. `qt_composed_github_scale_100`
  stops at BPR-0940's GitHub CLI card assertion,
  `qt_workbench_window_scale_100` at its epoch-row assertion, and
  `application_threaded_reply_contract` at its 40 s run budget, each
  identically before the repair (`build/bpr0985-baseline-qt.log`).
- **Registration.** In this checkout `test_network_isolation_manifest` reports
  the same four findings as at the merge head: three from the stray
  `.claude/worktrees/agent-a17cae88115dbbb4e` copy, plus [[BPR-0962]]'s
  `release_update_native_https` row. None is new. The structure contract
  passes in a clean export with this change
  (`build/bpr0985-isolation-structure-clean.log`).
- **Code checks.** clang-format is clean on every touched line (the one
  remaining warning in `workbench_window.hpp`, line 895, predates this card).
  clang-tidy on `github_conversation_panel.cpp`, `workbench_window.cpp` and
  `workbench_window_tests.cpp` reports only findings in untouched lines:
  `dif_runtime.hpp` `#pragma once`, `home_view_state.hpp` signed bitwise and
  the test's `wait_for_frame` name. `git diff --check` is clean.

Fixture-only; no live provider, credential or GitHub mutation. RFC-BPR-0024's
status is not changed here; the restored course is recorded on [[BPR-0595]].
