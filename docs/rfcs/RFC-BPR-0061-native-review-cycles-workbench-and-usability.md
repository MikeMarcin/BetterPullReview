---
id: "RFC-BPR-0061"
title: "Native Review Cycles Workbench and Usability"
status: "Proposed"
area: "BPR"
milestone: "Review Cycles"
depends_on: ["RFC-BPR-0053", "RFC-BPR-0054", "RFC-BPR-0055", "RFC-BPR-0056", "RFC-BPR-0057", "RFC-BPR-0052", "RFC-BPR-0037", "RFC-BPR-0050"]
pack_version: "review-cycles-v1"
metadata: {"pack": "bpr-review-cycles-v1", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# RFC-BPR-0061: Native Review Cycles Workbench and Usability

## Outcome and decisions

Present Review Cycles inside BPR as the source-first counterpart to PR review, not a separate product or an AI tools page. Add Open Project and local-cycle navigation with simple defaults. Preserve the owner-selected RFC0052 title/review bar, folding Updates line, document tabs and tray, adapting subject-specific actions without another permanent chrome row.

Full source is the initial and persistent mode. Inline notes, exact suggestions, related references and the ordinary composer build one feedback round. Share feedback exposes first-class Copy and Save; the optional external-skill and integrated routes add convenience without controlling lifecycle. A user never has to rewrite an already-authored review into a second prompt.

Capture revision and reviewer verification form the return path even without recipient reports. Per-tab material identity, review receipts, history and ambiguity remain visible. Publication preview shows the actual outgoing candidate, not a misleading local-only diff, and keeps local conversations private unless selected.

Use existing native keyboard/accessibility/editor behavior and conduct bounded attended design decisions where genuinely unresolved. This pack does not assert approval of new art or override the accepted scaffolding. Validate usability on actual supported platforms and measure human steering/reconstruction savings without assuming token or quality improvements from batching alone.

## Owner amendment, 2026-10-01: Suggest change in the main pane

The owner's decision on `BPR-1156-composer-layout-v003` is recorded in
[RFC-BPR-0055](RFC-BPR-0055-source-feedback-and-exact-suggested-edits.md#owner-amendment-2026-10-01-suggest-change-edits-the-file-in-place).
For this workbench it means: Suggest change is offered with only a caret, from
the review bar and the source pane's context menu; the whole file is edited in
the main pane rather than in the tray; and the tray's before/after colours its
removed and added lines the way the diff panes do. RC61-04 is amended to match.

## Normative contracts

The detailed contracts [UX.md](assets/bpr-review-cycles-v1/specs/UX.md), [LIMITS.md](assets/bpr-review-cycles-v1/specs/LIMITS.md) are normative parts of this proposal. The common [domain](assets/bpr-review-cycles-v1/specs/DOMAIN.md), [decisions](assets/bpr-review-cycles-v1/supporting/DECISIONS.md) and [security boundary](assets/bpr-review-cycles-v1/specs/SECURITY-AND-PACKAGING.md) apply. If a live predecessor differs, record a specific compatibility decision; do not quietly implement a weaker alternate workflow.

## Acceptance criteria

- **RC61-01:** Open Project/Review Cycles offers a simple no-account/no-agent local entry with a clear Local cycle identity distinct from PRs and Review Tours.
- **RC61-02:** Cycle controls integrate the 0052 title/review bar, folding Updates line, tabs and tray without a new unapproved persistent row or PR regression.
- **RC61-03:** Initial/full-source browsing is single-pane and remains available after comparisons; unchanged/historical local source stays commentable.
- **RC61-04:** Native note/suggestion workflows support primary/related anchors, accurate before/after, keyboard editing and explicit stale/ambiguous states; Suggest change starts from the caret, including the source pane's context menu, edits the whole file in the main pane, and previews removed and added lines in the diff panes' colours (owner amendment, 2026-10-01).
- **RC61-05:** Compose/Share prepares one complete assignment with first-class Copy and Save and no mandatory AI setup, second prompt or per-item ceremony.
- **RC61-06:** Whole-round feedback and exact delivery observations remain legible; optional direct/external routes add no hidden context or automatic dispatch.
- **RC61-07:** Capture, response display, comparison, reviewer verification and next-round composition work without callback/protocol prerequisites.
- **RC61-08:** Publication preview differentiates bases and selected final content and links the actual PR while preserving local history/privacy.
- **RC61-09:** Native width/focus/shortcuts/accessibility/IME/tab-restoration courses preserve accepted workbench design and record actual platform/user evidence.
- **RC61-10:** Named-corpus latency/memory and the eight-item/two-suggestion human-steering course measure benefit/limitations without unverified savings claims.

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
