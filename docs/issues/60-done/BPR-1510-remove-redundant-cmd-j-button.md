---
schema: "implement-rfc.card.v1"
id: "BPR-1510"
title: "Remove redundant Cmd+J button covered by Threads tab"
status: "Done"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Cmd+j button can go away, threads tab already covers the behavior","milestone_plan":{"id":"bpr1510-remove-cmd-j","path":"docs/rfcs/plans/bpr1510-remove-cmd-j/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8a4eee77607a1ef6d6bc0d3841bf2d5814bc2874","evidence":{"kind":"observation","detail":"Actual offscreen1280x820 and minimum900x420 headers inspected: no redundant widget or reserved gap. Coordinator clarified no human-only criterion exists; added implementation-plan gate corrected without changing original ACs."}},{"criterion":"AC2","outcome":"pass","revision":"8a4eee77607a1ef6d6bc0d3841bf2d5814bc2874","evidence":{"kind":"command","detail":"application_attended_recent_replay passes the Threads modality action through actual Composition; qt_workbench_conversation_reveal passes Threads tab and core transitions."}},{"criterion":"AC3","outcome":"pass","revision":"8a4eee77607a1ef6d6bc0d3841bf2d5814bc2874","evidence":{"kind":"command","detail":"qt_workbench_conversation_reveal passes actual Cmd+J key delivery and focus restoration; focus order passes 100/125/200; full Composition action/tab focus assertions pass once its window is explicitly activated."}},{"criterion":"AC4","outcome":"pass","revision":"8a4eee77607a1ef6d6bc0d3841bf2d5814bc2874","evidence":{"kind":"artifact","detail":"/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr1510-ashc70r3/focused-tests.log (first five pass) and /focus-course-recheck.log (Composition passes); actual offscreen Review and Threads PNGs inspected. Native system appearance is not inferred."}}]
---

# BPR-1510: Remove redundant Cmd+J button covered by Threads tab

Remove the visible Cmd+J button because the Threads tab already provides its behavior. Preserve the existing keyboard shortcut and Threads tab interaction; the owner requested removal of the button.

## Acceptance

- AC1: The redundant Cmd+J button is absent from the workbench and its former space does not leave an awkward layout gap.
- AC2: The Threads tab still provides the behavior previously reachable through the button.
- AC3: The existing Cmd+J keyboard shortcut continues to work, with no focus or accessibility regression from the removed widget.
- AC4: Verify actual offscreen rendering and exercise the Threads tab and keyboard action through the composed public path.

## Coordination

Load frontend-design:frontend-design before UX edits, as required by AGENTS.md. Coordinate shared tab/layout files with BPR-1509 (Files/Search tab styling), while preserving that card’s scope. Never drive or capture the owner’s live desktop. Any required human visual judgement uses a short attended session and frozen candidate.

## Milestone plan and checkpoint (2026-10-14)

- Assignment: BPR-1510 only, owner testing note 2026-10-14 r1; branch `bpr1510-remove-cmd-j`, original baseline `e0db7fb17`. Coordinator-authorized tracker imports through `9c80010a0` were copied unchanged; claim `4c99101a5`. No independent allocation.
- [Adopted plan](../../rfcs/plans/bpr1510-remove-cmd-j/milestone-plan.json): remove the redundant Comments-header button, retain Threads and Cmd+J, verify render/interaction/focus. The initial extra human gate is corrected by the [closeout clarification](../../rfcs/plans/bpr1510-remove-cmd-j/closeout-clarification.md); the original ACs are unchanged.
- Skills: implement-rfc, manage-agent-skills (exact registration plan `sha256:0452f63d3420c2a0c3c3f3ca8f5c4ffbf1e28094cfcd94a3aae88a980290ecf0` applied; preflight `ready_pinned`, host loading unverified), frontend-design:frontend-design.
- Design: preserve the native palette, typography, tab row and one-pixel tool spacing. Remove the finder widget outright so remaining comment tools occupy its former space.
- Scope: `workbench_window.cpp/.hpp`, existing window and composed application courses, and the owning UX specification. BPR-1509 confirmed its Files/Search styling hunks are independent.
- Documentation impact: UX specification updated for the removed entry. No policy, host, provider, persistence, publication, build or release behavior changes.
- Current state: all AC1–AC4 pass from the direct rendering and public interaction evidence below. The coordinator confirmed no explicit human-only criterion exists; the implementer-added gate is corrected without claiming a human verdict.
- Runtime evidence and milestone binding: `/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr1510-ashc70r3`. Completed build commands used isolated `build/dev`, four compile jobs, and shared dependency inputs. No active product writer, protected candidate or task-owned command remains.
- Next: coordinator integrates the scoped implementation and closeout commits. This bounded milestone requires no further owner action.

## Verified candidate and handoff (2026-10-14)

Source commits: `dcb4c478c` removes the widget/member/accessor and duplicate tooltip branch, updates its existing course and UX contract, and adopts the scoped plan. `e05a1d2ae` supplies the source-pane type include in the composition course; `8a4eee776` activates that offscreen window before assigning code focus. Those two corrections change only the test harness. App source bytes are unchanged after `dcb4c478c`.

Verification on macOS arm64, Qt 6.11.2:

- `cmake --preset dev -DCPM_SOURCE_CACHE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache -DCMAKE_TEST_BUILD_DEPENDS=OFF`, then a four-job build of `test_better_pull_review_qt_workbench_window_tests`, `test_better_pull_review_qt_github_tests`, `test_attended_recent_replay` and `app_better_pull_review`: pass after the missing include was fixed.
- `qt_composed_github_scale_100`, `qt_workbench_focus_order_scale_100`, `_125`, `_200`, and `qt_workbench_conversation_reveal`: pass. The latter delivers actual Cmd+J input, checks the Threads entry and return, and saves the full Review and Threads renders.
- `application_attended_recent_replay`: pass (31.60 s) with the real Composition action/tab/focus assertions. Its first run lacked an activated window and failed search-focus admission; the repaired course requires initial code focus, then verifies Threads search focus and restored code focus. No product change was needed.
- `architecture_boundaries`: pass (33.41 s). Touched-file format check passes with clang-format 22.1.8; `git diff --check` passes.
- `python3 scripts/verify_documentation.py --root .`: `DOCUMENTATION_OK` at `de0bfb4bc`. `python3 tests/scripts/verify_documentation_tests.py`: 67 pass. Later global board checks refuse the stale local counter as the coordinator imports more testing notes; this is recorded separately from product evidence.
- Actual `1280×820` Review/Threads renders and the composed minimum `900×420` render were inspected offscreen. The redundant button is absent, reading controls remain compact, and no fixed placeholder is left. These are layout observations, not native system-appearance acceptance.
- Self-review: all widget references are removed; the original QAction remains in View with its shortcut, enabled-state projection and existing transition. No host effect, policy, persistence or broader visual redesign was introduced.

## Retired preparation and completed upkeep (2026-10-14)

A frozen build at `/Users/jaeger/BPR-1510-cmd-j-v001.qdo6nre2` was prepared from source `de0bfb4bcc6e31d73de1be6a10cf134845789878`, executable SHA-256 `e0e4d380d1d76a4b3ef47e6b55d952320c79fc155f87a4ede859586ccab92e14`. Its own `launch.py --check` reached `ATTENDED_FILE_SHOWN:src/ui/conversation_cards_view.cpp`, head `89df5e592e5db97a526a05a445735733395e4832`. An initial preparation with the wrong file argument was stopped by exact owned PIDs before building and its root removed.

The coordinator confirmed that the original mandate/cards contain no explicit human-only criterion and directed correction of the added gate using the passing offscreen evidence. The unnecessary session `40c921d030490283d544fb88816130ed` was inspected with no verdict, released at cursor `3`, and stopped through the attended companion; server PID `95114` and command-session `71176` have exited. Its frozen root was removed through `freeze_attended_build.py remove`. There is no active review URL, protection, pending decision or waiter. Historical identities and the superseded handoff remain in private runtime evidence.

Coordinator-authorized metadata-only imports `fa776388e..3e8793add` were inspected and copied unchanged: cards, allocation receipts/counter, and owner-note amendments only. Local NEXT_ID is BPR-1521; no independent allocation or main edit occurred. The prior stale-counter limitation is resolved.

Final acceptance: AC1 passes from actual default and minimum rendered headers with no button, reserved spacer or awkward gap; AC2–AC4 pass from the unchanged focused courses recorded above. Product and test inputs have not changed since the passing checks, so their evidence is reused. The final documentation gate is rerun for the plan correction and board closeout.

Ownership: all task-owned commands, writers and protections are quiescent. No owner desktop was driven or captured; no live GitHub effect occurred. The final closeout binding, observation, acceptance records and gate output are retained in `/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr1510-ashc70r3`. The coordinator's next action is integration; no in-scope obligation remains.
