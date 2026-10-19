---
schema: "implement-rfc.card.v1"
id: "BPR-1662"
title: "Verify and close RFC-BPR-0087"
status: "Blocked"
rfcs: ["RFC-BPR-0087"]
depends_on: ["BPR-1659", "BPR-1660", "BPR-1661"]
metadata: {"source": "Owner request 2026-10-19; isolated worktree, no merge/push/release", "milestone_plan": {"id": "shared-action-buttons-v1", "path": "docs/rfcs/plans/shared-action-buttons-v1/milestone-plan.json"}}
closeout_for: ["RFC-BPR-0087"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"81a13267446e883c12ac0f8c7b5a43927c2efc68","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0087.md","detail":"Pass for recorded offscreen scope; `9dc946997350ab23866ce75f39bbebf32b140be8` retains the reference/shared Settings treatment; 10/10 focused and 18/18 four-theme style/scale courses pass. See [criterion judgment](../../verification/RFC-BPR-0087.md#criterion-judgment)."}},{"criterion":"AC02","outcome":"pass","revision":"81a13267446e883c12ac0f8c7b5a43927c2efc68","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0087.md","detail":"Pass; `81a13267446e883c12ac0f8c7b5a43927c2efc68` completes the [46-row inventory](../../ux/shared-action-buttons.md), including mixed factories and concrete retained-variant reasons. Broader baseline scan also accounts for four specialized implementation units and one header; none is deferred."}},{"criterion":"AC03","outcome":"pass","revision":"81a13267446e883c12ac0f8c7b5a43927c2efc68","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0087.md","detail":"Pass for exercised Qt contracts; `81a13267446e883c12ac0f8c7b5a43927c2efc68` has 23/23 focused and tour-writing passes, plus final 4/4 smoke at `bb64e195d93b2a46e35352659cd6e23686337ed5`: keyboard/default/exclusive/disabled, accessibility, icon/menu/native fallback, translated/font sizing and theme changes. Existing names, tooltips, focus policy and dispatch retained."}},{"criterion":"AC04","outcome":"pass","revision":"81a13267446e883c12ac0f8c7b5a43927c2efc68","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0087.md","detail":"Pass for offscreen drawing/layout; first-slice 18/18 and app-rollout 6/6 four-theme courses span 1x/1.25x/2x Fusion/Windows styles with actual composed state captures inspected. Native macOS/Windows/Linux and owner visual verdict remain unobserved. Unrelated full Appearance Fusion-2x popover failure and native-choice reproduction retained explicitly."}},{"criterion":"AC05","outcome":"pass","revision":"81a13267446e883c12ac0f8c7b5a43927c2efc68","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0087.md","detail":"Pass technically; final 4/4 smoke (7.39 s), clang-format 22.1.8 on all 47 changed C++ paths, architecture, documentation and scoped board gates pass. Evidence uses unchanged source `81a13267446e883c12ac0f8c7b5a43927c2efc68`; exact commands/results are in the [ledger](../../verification/RFC-BPR-0087.md#final-verification-and-closeout-proposal). Separate integration authorization remains pending."}},{"criterion":"BPR-1662:integration-authorization","outcome":"pending","revision":"81a13267446e883c12ac0f8c7b5a43927c2efc68","evidence":{"kind":"observation","detail":"Separate integration authorization absent; explicit user boundary prohibits merge, push and release. No integration action attempted."}}]
blocked_by: "integration-authorization"
---

# BPR-1662: Verify and close RFC-BPR-0087

## Acceptance

- Judge every RFC criterion against direct evidence, with the full migration inventory accounted for.
- Documentation gate, focused integration and relevant formatting checks pass; retain native-platform limitations and obtain separate integration authorization.

RFC base: `74ef7f4fb643b2fbbcd4753efcd1fe29c38bbddc`.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01:** Overview's selected/unselected Open/All and the migrated Settings choices/actions visibly use one shared treatment in all four themes, without the heavy native bezel or changes to labels and meaning. | Pass for recorded offscreen scope; `9dc946997350ab23866ce75f39bbebf32b140be8` retains the reference/shared Settings treatment; 10/10 focused and 18/18 four-theme style/scale courses pass. See [criterion judgment](../../verification/RFC-BPR-0087.md#criterion-judgment). |
| **AC02:** The app-wide inventory accounts for comparable text buttons on every Qt surface. Each is migrated or has an intentional variant reason; deferred migrations are visible and prevent app-wide closeout. | Pass; `81a13267446e883c12ac0f8c7b5a43927c2efc68` completes the [46-row inventory](../../ux/shared-action-buttons.md), including mixed factories and concrete retained-variant reasons. Broader baseline scan also accounts for four specialized implementation units and one header; none is deferred. |
| **AC03:** Existing gestures and command outcomes still hold. Keyboard focus, exclusivity, checked state, disabled state, accessibility, icon/menu content and translated sizing survive adoption, including theme/style changes. | Pass for exercised Qt contracts; `81a13267446e883c12ac0f8c7b5a43927c2efc68` has 23/23 focused and tour-writing passes, plus final 4/4 smoke at `bb64e195d93b2a46e35352659cd6e23686337ed5`: keyboard/default/exclusive/disabled, accessibility, icon/menu/native fallback, translated/font sizing and theme changes. Existing names, tooltips, focus policy and dispatch retained. |
| **AC04:** Representative composed Overview, Settings Review/Comments and another migrated page are rendered and inspected with hover, press, focus and disabled evidence at 1x, 1.25x and 2x. Available style variants are exercised; offscreen and native-platform claims are recorded separately. | Pass for offscreen drawing/layout; first-slice 18/18 and app-rollout 6/6 four-theme courses span 1x/1.25x/2x Fusion/Windows styles with actual composed state captures inspected. Native macOS/Windows/Linux and owner visual verdict remain unobserved. Unrelated full Appearance Fusion-2x popover failure and native-choice reproduction retained explicitly. |
| **AC05:** Focused composition and presentation checks, formatting and the documentation gate pass. The verification record names actual commands, revision, observed results, retained variants and remaining platform work. | Pass technically; final 4/4 smoke (7.39 s), clang-format 22.1.8 on all 47 changed C++ paths, architecture, documentation and scoped board gates pass. Evidence uses unchanged source `81a13267446e883c12ac0f8c7b5a43927c2efc68`; exact commands/results are in the [ledger](../../verification/RFC-BPR-0087.md#final-verification-and-closeout-proposal). Separate integration authorization remains pending. |

## Historical first source checkpoint

`2660e4c63096479bb5d78090017e9f0e8d898e28` begins the shared painter and
Settings migration. [Verification and exact pending checks](../../verification/RFC-BPR-0087.md);
[app-wide inventory](../../ux/shared-action-buttons.md). At that checkpoint no criterion was closed from syntax/static checks and
BPR-1659 was blocked on disk space. That pause was resolved; BPR-1659, BPR-1660
and BPR-1661 are now Done with runtime and inspection evidence. The current
frontier is the closeout proposal below.

## Execution record

Technical assessment is complete for source
`81a13267446e883c12ac0f8c7b5a43927c2efc68`, with first-slice evidence at
`9dc946997350ab23866ce75f39bbebf32b140be8` and final 4/4 smoke at
`bb64e195d93b2a46e35352659cd6e23686337ed5`. BPR-1661 completed at
`2e808fe675eacd377bfc3ff1770efb391a0c9332`. The final source diff was judged
against the immutable RFC base above; no deferred migration or known in-scope
regression remains. The [ledger](../../verification/RFC-BPR-0087.md#final-verification-and-closeout-proposal)
records exact commands, inspected captures and native-platform limitations.

## Remaining blocker

**`integration-authorization` — pending, not denied or attempted.** The second
acceptance bullet explicitly also requires separate integration authorization.
The user permits the implementation/verification rollout and expressly says no
merge, push or release. No such action was attempted, and no approval is
invented or requested by this proposal. No source or test work remains for this
card within the current authorization. Native desktop outcomes remain accurately
unobserved; they are not silently converted to failures or passes.

Preserve the reviewed isolated branch, build and local evidence. A future
explicitly authorized integration owner must reconcile the exact candidate and
then resolve this remaining closeout obligation. Until then this card is
Blocked and RFC-BPR-0087 and its index stay In Progress. No attended review or
wizard is created because no native/human judgment is required by this card;
the only pending obligation is authorization outside this task's stop boundary.
