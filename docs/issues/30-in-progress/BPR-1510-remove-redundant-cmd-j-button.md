---
schema: "implement-rfc.card.v1"
id: "BPR-1510"
title: "Remove redundant Cmd+J button covered by Threads tab"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Cmd+j button can go away, threads tab already covers the behavior","milestone_plan":{"id":"bpr1510-remove-cmd-j","path":"docs/rfcs/plans/bpr1510-remove-cmd-j/milestone-plan.json"}}
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
- [Adopted plan](../../rfcs/plans/bpr1510-remove-cmd-j/milestone-plan.json): remove the redundant Comments-header button, retain Threads and Cmd+J, verify render/interaction/focus, then present AC1's visual judgment as one short attended candidate.
- Skills: implement-rfc, manage-agent-skills (exact registration plan `sha256:0452f63d3420c2a0c3c3f3ca8f5c4ffbf1e28094cfcd94a3aae88a980290ecf0` applied; preflight `ready_pinned`, host loading unverified), frontend-design:frontend-design.
- Design: preserve the native palette, typography, tab row and one-pixel tool spacing. Remove the finder widget outright so remaining comment tools occupy its former space.
- Scope: `workbench_window.cpp/.hpp`, existing window and composed application courses, and the owning UX specification. BPR-1509 confirmed its Files/Search styling hunks are independent.
- Documentation impact: UX specification updated for the removed entry. No policy, host, provider, persistence, publication, build or release behavior changes.
- Current state: implementation and automated AC2–AC4 verified. AC1 absence and spacing are directly observed offscreen; the initially authored plan also introduced a human visual ask, whose necessity is awaiting coordinator reconciliation against the original card. No human verdict is claimed.
- Runtime evidence and milestone binding: `/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr1510-ashc70r3`. Task-owned build command uses the isolated `build/dev`, four compile jobs, shared read-only dependency inputs. No other product writer or protected candidate yet.
- Next: coordinator reconciles the added visual gate, supplies the current import-only tracker snapshot, integrates the scoped commits and runs canonical finish. If the AC1 ask is retained, use the prepared short session and a fresh post-review implementer; otherwise explicitly correct the added plan gate, release the candidate and finish from the objective evidence.

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

Frozen candidate: `/Users/jaeger/BPR-1510-cmd-j-v001.qdo6nre2`, source `de0bfb4bcc6e31d73de1be6a10cf134845789878`, executable SHA-256 `e0e4d380d1d76a4b3ef47e6b55d952320c79fc155f87a4ede859586ccab92e14`. `scripts/freeze_attended_build.py freeze` exported committed fixtures and built inside that root; its own `launch.py --check` passed with `ATTENDED_FILE_SHOWN:src/ui/conversation_cards_view.cpp`, head `89df5e592e5db97a526a05a445735733395e4832`. An initial preparation with the wrong file argument was stopped by its exact task-owned PIDs before building and its root removed.

Launch from any directory:

```sh
python3 /Users/jaeger/BPR-1510-cmd-j-v001.qdo6nre2/launch.py
```

Short attended session (prepared under the current plan; no verdict supplied):

- State: `/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr1510-ashc70r3/attended-review`. Default resource registry: `/Users/jaeger/.mask/attended-review-resources`.
- Session `40c921d030490283d544fb88816130ed`, cursor `2`, candidate `BPR-1510-header-v001`, fingerprint `4fd4a35d5d58604ac69a69a777f133b70dd1110c12a374ce082f975e78ef3b8c`. Candidate is ready/protected. Only ask: AC1 resulting Comments-header spacing, Accept/Reject.
- Loopback host port `52575`, PID `95114`, command-session `71176`. Complete token-bearing URL is in the private runtime handoff and coordinator message, not committed.
- Coordinator owns any attended wait: run the companion with the same source/state and `wait --after 2 --kind handed-back --timeout 43200`; validate this session and consume new events before dispatching the next round. No waiter was started by this leaf.

Ownership: engineering writer released after this checkpoint; all build, test, configure and freeze commands have completed. Only the explicitly handed-off review server remains. The two protected source paths and frozen-build resource are listed in the session. No owner desktop was driven or captured; no live GitHub effect occurred.

Tracker limitation: authorized imports through `fa776388e` were copied unchanged. Canonical `finish --dry-run` refuses local `NEXT_ID` BPR-1516 against historical BPR-1520. No unapproved counter/receipt bytes were changed. The card stays In Progress until the coordinator supplies the current approved tracker snapshot or finishes after integration. Acceptance decision file, commands, images and exact checkpoint records are retained in the runtime evidence root above.
