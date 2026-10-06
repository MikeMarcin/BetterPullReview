---
id: "RFC-BPR-0057"
title: "Local Iteration, Recipient Responses and Reviewer Verification"
status: "Auto-Verified"
area: "BPR"
milestone: "Review Cycles"
depends_on: ["RFC-BPR-0053", "RFC-BPR-0054", "RFC-BPR-0055", "RFC-BPR-0056"]
pack_version: "review-cycles-v1"
metadata: {"pack": "bpr-review-cycles-v1", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# RFC-BPR-0057: Local Iteration, Recipient Responses and Reviewer Verification

## Outcome and decisions

Advance a Review Cycle through captured code revisions regardless of how feedback was delivered or whether a recipient sends a structured report. A colleague or external harness can edit with ordinary tools, and the reviewer can capture the next revision even after BPR was closed. No agent completion protocol unlocks the loop.

Provide Full source, This round, Since my last review, All cycle changes and exact revision-pair semantics. Distinguish review checkpoints from file visits and reported work completion. Preserve revision DAG/coverage uncertainty and pinned tab identities as new material arrives.

Optional structured responses append item-linked statements with exact round/digest and provenance. Manual prose works equally well. Imported claims are not source observations, verified model identity, tested evidence or authorization. Replayed reports are idempotent; conflicts remain visible.

Map current material separately from original anchors and distinguish recipient addressed claims from explicit reviewer verification. Verification binds item/source versions and loses current applicability when relevant material changes without erasing the historical judgment. Next-round authoring carries forward selected unresolved issues and context, not every private turn automatically.

## Normative contracts

The detailed contracts [ITERATION.md](assets/bpr-review-cycles-v1/specs/ITERATION.md) are normative parts of this proposal. The common [domain](assets/bpr-review-cycles-v1/specs/DOMAIN.md), [decisions](assets/bpr-review-cycles-v1/supporting/DECISIONS.md) and [security boundary](assets/bpr-review-cycles-v1/specs/SECURITY-AND-PACKAGING.md) apply. If a live predecessor differs, record a specific compatibility decision; do not quietly implement a weaker alternate workflow.

## Acceptance criteria

- **RC57-01:** Capture R1/R2 and continue manually without delivery receipt, structured report, BPR-managed agent, Git commit or completion callback.
- **RC57-02:** Full source/This round/Since my last review/All cycle/exact-pair views resolve the intended immutable material and never retarget explicit tabs.
- **RC57-03:** Reconciliation distinguishes exact, ambiguous, deleted and unavailable source and never turns proximity/code disappearance into proof of resolution.
- **RC57-04:** Reports/manual replies attach to exact rounds/items as append-only untrusted recipient statements with observed provenance.
- **RC57-05:** Identical report replay is idempotent; reused report IDs with different bytes and late/wrong-cycle output conflict or refuse.
- **RC57-06:** Recipient addressed status and reviewer verification are separate; verification binds checked item/material and becomes visibly historical when relevant code changes.
- **RC57-07:** Next-round selection/amendment preserves prior assignments and can include unresolved/new feedback without auto-dispatch or forced per-item interaction.
- **RC57-08:** Human-only, mixed-recipient, partial-response and prose-only returns work without JSON/protocol requirements or invented missing responses.
- **RC57-09:** Local capture order, imported source DAG and provider upload chronology remain separate, with gaps/divergence explicit.
- **RC57-10:** Claimed code/test outcomes remain reported evidence; importing them executes no tests, writes no source and grants no verification/publication authority.

## Implementation and evidence

Use the associated authored cards and imported milestone grouping. New names/types/paths
are proposals until reconciled with the live tree. Preserve functional-core/host/Qt
ownership and existing test isolation. Implement focused production-path unit/host/
native checks; use unchanged predecessor evidence where inputs have not changed.
Source inspection, reference fixtures and this archive's validator are not runtime
implementation, native/platform behavior, live provider acceptance or user approval.

Record command/gesture, result, producing revision, environment and limitations for
each acceptance criterion. Required live or attended observations stay open until
actually performed on authorized targets. Do not start models, alter shared skills,
write live repositories, or bypass corporate policy merely because a test card exists.
Keep historical RFC status/evidence unchanged; update current architecture forward.

See the [integration map](assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md), [acceptance matrix](assets/bpr-review-cycles-v1/verification/ACCEPTANCE-MATRIX.md) and [product verification contract](assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md).
