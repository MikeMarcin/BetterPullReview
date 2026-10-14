---
schema: "implement-rfc.card.v1"
id: "BPR-1511"
title: "Scope unread comments badge to the current file"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Badge for unread comments should only show when there is one on the current file","milestone_plan":{"id":"bpr1511-current-file-unread-badge","path":"docs/rfcs/plans/bpr1511-current-file-unread-badge/milestone-plan.json"}}
---

# BPR-1511: Scope unread comments badge to the current file

Show the file-context unread-comments badge only when the currently selected file has an unread comment. Unread comments on other files must not make this badge appear.

## Acceptance

- AC1: The badge appears when the current file has unread comments and is absent when that file has none, even when another file has unread comments.
- AC2: Any count shown by this badge reflects only the current file’s unread comments, under the existing unread policy.
- AC3: File changes, marking comments read, refresh and authority replacement update the badge correctly without stale counts or cross-file leakage. With no current file the badge is absent.
- AC4: Verify the composed file-selection and unread-update paths with representative multiple-file comments and an offscreen rendered state.

## Coordination

Identify the existing file-context badge before changing it; retain distinct PR-wide indicators and existing unread semantics. Load frontend-design:frontend-design before UX edits as required by AGENTS.md. Coordinate shared tab/comment projection paths with BPR-1509 and BPR-1510. Never operate or capture the owner’s live desktop. Required visual judgement goes through a short attended session and frozen candidate.

## Verification and execution record

2026-10-14, mandate `owner-2026-10-14-current-file`, candidate
`879d3c6b7632a4f4c7919697cdf53f755b254fab`:

- AC1/AC2: title comments toggle and Comments-tab pill use the current file's
  unread-thread count. A three-file course observes counts 2, 1 and absent;
  multiple unread posts still count as one thread under the existing policy,
  and hiding resolved threads does not change that policy. PR-wide Next new
  comment and Discuss's separate indicators retain their existing meanings.
- AC3: composed semantic file selection, Mark read dispatch, new unread update,
  full unread projection, refresh gap, accepted conversation replacement,
  explicit empty selection with an old diff retained, authority invalidation
  and replacement all pass. The visual anchor retained during a refresh gap
  cannot supply a file badge. Single-thread read updates adjust the affected
  cached file count without rebuilding the review.
- AC4: final focused CTest pass is 5/5: `qt_current_file_unread_badge`,
  `qt_conversation_cards_view`, `qt_composed_github_scale_100`,
  `qt_workbench_conversation_reveal`, `architecture_boundaries` (34.25 s).
  Full offscreen unread-file and clean-file windows were rendered and
  inspected; ignored paths and digests are recorded in the checkpoint.
- Negative control: temporarily using the review-wide count on a selected
  file fails the first-file count assertion. The restored candidate passes.
  An initial regression found action refresh overwriting the scoped accessible
  wording; refreshing badges after drawer actions fixes it and the course
  verifies both accessible descriptions.
- All six touched C++ files pass clang-format 22.1.8 checks; `git diff --check`
  passes; documentation verifier tests pass 67/67. Initial documentation gate
  is held only by new shared issue allocations beyond this worktree's counter;
  no manual counter or reservation edit was made here.

The [durable handoff](../_checkpoints/BPR-1511/handoff.md),
[criterion acceptance](../_checkpoints/BPR-1511/acceptance.json),
[render evidence](../_checkpoints/BPR-1511/render-evidence.json) and
[milestone observation](../_checkpoints/BPR-1511/milestone-observation.json)
bind the exact assignment, source candidate, commands, paths and next action.
Frontend-design was loaded before UX changes; inherited native styling and
existing badge geometry were preserved. No owner desktop access or capture.
No new human/native criterion is required by this card; parent RFC obligations
remain unchanged. All owned commands are finished; no review server or other
background writer is running. Coordinator integrates at this card boundary.

The shipped-app `application_diff_workbench_public_path` and
`application_workbench_composition_contract` also pass (2/2, 17.32 s), including
the real installed-Git/cache selected-file trace and cut-selection-wire
negative control. The full application render was inspected. Canonical finish
and documentation gate remain pending only the coordinator-authorized tracker
imports `75dfc87c5..9b551b738`; the final dry-run refuses NEXT1517 against
historical NEXT1520. No new allocation is needed.
