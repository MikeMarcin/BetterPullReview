---
schema: "implement-rfc.card.v1"
id: "BPR-1518"
title: "Navigate to the next change after the clicked cursor position"
status: "Done"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"If we click a line below a change hunk next change goes to the first change in the file. Should be the next change after the cursor probably","dispatch":"Dedicated leaf /root/bpr1518 on codex/bpr1518-next-change","milestone_plan":{"id":"bpr1518-next-change","path":"docs/rfcs/plans/bpr1518-next-change/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c703ed29200eaf07d620cdc669678fd02937c5ed","evidence":{"kind":"observation","detail":"Actual unchanged-line clicks between hunks followed by composed Next Change choose the following hunk. Restoring group-only origin makes the exact hunk/source assertion fail.","reference":"docs/issues/_checkpoints/BPR-1518/verification.json"}},{"criterion":"AC2","outcome":"pass","revision":"c703ed29200eaf07d620cdc669678fd02937c5ed","evidence":{"kind":"observation","detail":"46 exact file/side/source click origins in17 files include unequal mappings; each action asserts the canonical target selection, exact comparison and visible source anchor.","reference":"docs/issues/_checkpoints/BPR-1518/verification.json"}},{"criterion":"AC3","outcome":"pass","revision":"c703ed29200eaf07d620cdc669678fd02937c5ed","evidence":{"kind":"observation","detail":"Core and composed checks cover before-first, inside endpoint groups, gaps and after-last; Previous is symmetric, bounded actions keep source/viewport, fixture wrap remains explicit. Owning UX contract updated.","reference":"docs/issues/_checkpoints/BPR-1518/verification.json"}},{"criterion":"AC4","outcome":"pass","revision":"c703ed29200eaf07d620cdc669678fd02937c5ed","evidence":{"kind":"observation","detail":"Real QMouseEvent source clicks replace prior navigated groups before real window QActions. Same-file base/base replacement refuses the stale selection and preserves the new source origin/no-changes bound.","reference":"docs/issues/_checkpoints/BPR-1518/verification.json"}}]
---

# BPR-1518: Navigate to the next change after the clicked cursor position

Clicking a source line below a change hunk and invoking Next Change currently returns to the first change in the file. Use the current clicked cursor position as the navigation origin so Next Change selects the next change after it. Treat the owner’s suggestion as the intended direction; inspect existing endpoint/wrap semantics and make them explicit rather than silently inventing a new policy.

## Acceptance

- AC1: Reproduce clicking a line between hunks and invoking Next Change; select the next hunk after the cursor rather than the first hunk in the file.
- AC2: Navigation uses the correct current file, side and semantic source position, including unchanged lines outside a hunk and unequal left/right mappings.
- AC3: Before-first, inside-hunk and after-last positions have coherent documented behavior consistent with the existing navigation contract; inspect Previous Change for the same stale-origin problem and preserve directional symmetry where appropriate.
- AC4: Verify actual source-line click then composed Next Change action, asserting selected hunk/source identity and visible position; cover cursor replacement after prior hunk navigation and stale comparison changes.

## Coordination

BPR-1517 tracks stalled Next/Previous File actions as a separate issue; preserve distinct scope while sharing navigation findings. Coordinate any diff cursor/splitter overlap with BPR-1516. Load frontend-design:frontend-design before UX edits per AGENTS.md. Never operate or capture the owner’s live desktop. No remote mutation needed.

## Verification and result

Candidate `c703ed29200eaf07d620cdc669678fd02937c5ed` passes AC1–AC4.
The existing composed change-arrow course now drives 46 actual source clicks
on both sides, including unchanged gaps, both endpoint groups and unequal line
mappings, followed by real window actions. It asserts exact source/group identity,
visible target anchors, bounded source/viewport retention, cursor replacement and
a same-file base/base comparison change that refuses the stale selection.
The course passes in 50.14 seconds on macOS arm64, Qt 6.11.2 offscreen. Replacing
only the core line-origin branch with the old group-only behavior makes this
course fail at its exact hunk/source assertion; the fixed bytes are restored.

Focused dev checks pass 3/3 and Qt-free core/selected-graph checks pass 3/3.
Both architecture checks, changed-core clang-tidy, clang-format 22.1.8, the
documentation gate and all 67 verifier tests pass. The unchanged wider contract
checks are reused. See [acceptance](../_checkpoints/BPR-1518/acceptance.json),
[verification](../_checkpoints/BPR-1518/verification.json) and
[checkpoint](../_checkpoints/BPR-1518/continuation.md).

The owning [workbench contract](../../ux/workbench-specification.md) now states
source-relative Previous/Next behavior and preserves bounded/no-wrap policy.
Frontend-design was loaded before UX edits; no Qt geometry or layout change was
needed. No human, native or independent decision is required by this card.
