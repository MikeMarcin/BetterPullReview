---
id: "RFC-BPR-0055"
title: "Source Feedback, Provenance and Exact Suggested Edits"
status: "Proposed"
area: "BPR"
milestone: "Review Cycles"
depends_on: ["RFC-BPR-0053", "RFC-BPR-0054", "RFC-BPR-0036", "RFC-BPR-0047", "RFC-BPR-0048"]
pack_version: "review-cycles-v1"
metadata: {"pack": "bpr-review-cycles-v1", "proposed_id_requires_live_reconciliation": true, "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# RFC-BPR-0055: Source Feedback, Provenance and Exact Suggested Edits

## Outcome and decisions

Make writing feedback in source the principal authoring interaction. A note may target a range, file, module or cycle and can reference several related locations. Original source attachment is immutable; a current correspondence is separately derived and may be ambiguous. The user can comment on unchanged or historical code without an invented PR hunk.

Offer a focused Suggest change editor prefilled from the selection. Persist exact original/replacement bytes and coordinates, including insertion/deletion and line-ending details. Distinguish illustrative snippets, exact proposals and observed already-applied local edits. Preview does not edit the developer's working tree. Full-set overlap/precondition checks precede any supported application; no fuzzy nearest-text patching.

Reuse shared editors, native Markdown and all twelve optional Conventional Comments labels. Preserve the user's virtual-space/multi-caret ergonomics and ordinary undo. RFC0036 concerns rendering, not proof the desired multi-caret editor is delivered. Inventory the actual editor predecessor and fill only necessary gaps.

Private cycle conversations need versioned human/agent/unknown provenance now. Reuse the minimal provider-independent seam of deferred RFC0023, or the live component if it has landed, without silently activating its full deferred program or creating a second incompatible thread store. Authored text stays authoritative and freeze/export needs no AI rewriting.

## Normative contracts

The detailed contracts [FEEDBACK.md](../../../specs/FEEDBACK.md) are normative parts of this proposal. The common [domain](../../../specs/DOMAIN.md), [decisions](../../../supporting/DECISIONS.md) and [security boundary](../../../specs/SECURITY-AND-PACKAGING.md) apply. If a live predecessor differs, record a specific compatibility decision; do not quietly implement a weaker alternate workflow.

## Acceptance criteria

- **RC55-01:** Author range/file/module/cycle notes and one multi-location discussion from full source or a diff without mandatory per-item metadata.
- **RC55-02:** Original source and ranges remain immutable; current exact/ambiguous/deleted/unavailable correspondence is separately represented.
- **RC55-03:** Plain and all twelve shared labels preserve authored Markdown and do not imply patches, severity, permissions or verification.
- **RC55-04:** Suggestion authoring preserves original/replacement bytes, insertion/deletion, explicit intent, byte ranges and before/after preview.
- **RC55-05:** Exact application refuses stale whole-blob preconditions and overlapping/same-boundary edits; nonoverlapping edits use original coordinates.
- **RC55-06:** Editing a suggestion does not write the checkout; actually applied edits are observed/labeled instead of dispatched for double application.
- **RC55-07:** Native IME, undo, multi-caret/virtual-space and clipboard workflows preserve exact text and selections in the suggestion composer.
- **RC55-08:** Human, adapter-observed, self-reported and unknown provenance remain distinct and private unless selected for disclosure.
- **RC55-09:** Whole-round preparation preserves the reviewer wording and suggestions without a required model call, summarizer or second combined prompt.
- **RC55-10:** Cycle threads reuse the shared narrow private-thread/lineage seam and do not fork or falsely complete deferred RFC0023.

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

See the [integration map](../../../supporting/INTEGRATION-MAP.md), [acceptance matrix](../../../verification/ACCEPTANCE-MATRIX.md) and [product verification contract](../../../verification/PRODUCT-VERIFICATION.md).
