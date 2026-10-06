---
id: "RFC-BPR-0055"
title: "Source Feedback, Provenance and Exact Suggested Edits"
status: "Auto-Verified"
area: "BPR"
milestone: "Review Cycles"
depends_on: ["RFC-BPR-0053", "RFC-BPR-0054"]
pack_version: "review-cycles-v1"
metadata: {"pack": "bpr-review-cycles-v1", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}, "owner_dependency_amendment": {"date": "2026-10-06", "removed_closure_dependencies": ["RFC-BPR-0036", "RFC-BPR-0047", "RFC-BPR-0048"], "uses": ["BPR-1076", "BPR-1082", "BPR-1083", "RFC-BPR-0036 native Markdown reader"], "card": "BPR-1188"}}
---

# RFC-BPR-0055: Source Feedback, Provenance and Exact Suggested Edits

## Outcome and decisions

Make writing feedback in source the principal authoring interaction. A note may target a range, file, module or cycle and can reference several related locations. Original source attachment is immutable; a current correspondence is separately derived and may be ambiguous. The user can comment on unchanged or historical code without an invented PR hunk.

Offer Suggest change from the caret; it edits the whole file in place in the main pane (owner amendment below). Persist exact original/replacement bytes and coordinates, including insertion/deletion and line-ending details. Distinguish illustrative snippets, exact proposals and observed already-applied local edits. Preview does not edit the developer's working tree. Full-set overlap/precondition checks precede any supported application; no fuzzy nearest-text patching.

Reuse shared editors, native Markdown and all twelve optional Conventional Comments labels. Preserve the user's virtual-space/multi-caret ergonomics and ordinary undo. RFC0036 concerns rendering, not proof the desired multi-caret editor is delivered. Inventory the actual editor predecessor and fill only necessary gaps.

Private cycle conversations need versioned human/agent/unknown provenance now. Reuse the minimal provider-independent seam of deferred RFC0023, or the live component if it has landed, without silently activating its full deferred program or creating a second incompatible thread store. Authored text stays authoritative and freeze/export needs no AI rewriting.

## Owner amendment, 2026-10-01: Suggest change edits the file in place

The owner rejected `BPR-1156-composer-layout-v003` (attended session
`cbd48b91000b00f1e72e1ad62c7c384c`, event 742) with this decision:

> If you're going to show +/- lines in the bottom right they should be colored
> like we do our diffs.
>
> I should be able to right click suggest change. I'm not sure why i can't
> suggest change without a selected block, I feel like a cursor should be
> sufficient.
>
> I don't want to edit in a tiny box on the right I want to edit in the main
> window. I don't want to be limited to changing the lines I initially selected
> either, can't I just suggest change and start editing in that whole file?

This changes the shape of Suggest change. Where the imported
[FEEDBACK.md](assets/bpr-review-cycles-v1/specs/FEEDBACK.md) ("a focused code
editor prefilled from the selected bytes ... changes that small section") and
[UX.md](assets/bpr-review-cycles-v1/specs/UX.md) ("a selected range supports
... Suggest change"; "the inline editor starts with exact selected code") say
otherwise, this amendment governs; the imported packet stays byte for byte.

- **Entry.** Suggest change needs only the caret. It is offered from the review
  bar and from the source pane's context menu; a selection, if there is one, is
  selected again in the editor.
- **Where.** The whole captured file is edited in the main pane, in the same
  multi-caret editor (RC55-07). The tray beside it keeps the location, intent,
  rationale and the before/after. Nothing is written to the checkout.
- **Extent.** Each separate changed run of whole lines, compared with the
  captured file line by line with each line's own ending, is one exact
  suggestion in the original blob's coordinates. Runs are separated by at least
  one unchanged line, so no two overlap or share a boundary; applying all of
  them gives exactly the edited file.
- **Delivery.** All the runs of one edit are suggestions of one note, anchored
  at the first run. A frozen round already carries each suggestion as its own
  suggested edit with its own range and exact original and replacement bytes,
  and the existing preflight applies the set or refuses it whole. This is also
  GitHub's model, where one suggestion block replaces one contiguous run of
  lines, so each run would map to its own suggestion if a later publication
  route offered one. A local cycle publishes nothing to GitHub.
- **Preview.** The before/after shows each change with nearby unchanged lines,
  and its removed and added rows wear the diff panes' own fills and syntax
  colours.
- **Bounds.** A file larger than 1 MiB is not opened for editing; Comment
  remains. More than 64 separate changes, or one change larger than 64 KiB,
  cannot be saved, and the composer says why instead of cutting anything.

The owner's rejection of `BPR-1156-composer-layout-v004` (event 765) does not
change this decision. It asks that reading and editing be one code view whose
text does not change when Suggest change begins, with one selection behaviour
in which selected code keeps its syntax colours, and that Enter indent like a
modern editor (RC55-07's editing workflows): the new line keeps the line's
indentation and adds a level after an opener.

The owner's rejection of `BPR-1156-composer-layout-v005` (event 776) refines
"the user's virtual-space ergonomics" for this composer: virtual space belongs
to the box selection (Alt+Shift+Arrow) alone, and every other gesture keeps the
caret on a line's text. It also asks that a selection read like a normal
editor's, and that a double click select the word.

Observations and evidence for this amendment are recorded on
[BPR-1156](../issues/60-done/BPR-1156-deliver-the-native-focused-suggestion-editor.md)
and in the [execution ledger](../verification/RFC-BPR-0053-0061.md), not here.

## Owner amendment, 2026-10-06: dependencies are what this RFC uses

As imported, this RFC listed RFC-BPR-0036, 0047 and 0048 among its
dependencies. The owner was asked in chat whether to close it under a
dependency amendment or wait for those three RFCs to close ("RFC-0055 passes
all 10 criteria but formally depends on RFC-0036/0047/0048 … Amend and close /
Wait"), and answered:

> Amend and close (Recommended)

RFC-BPR-0055 depends on what it uses, not on the closure of RFC-BPR-0036, 0047
and 0048:

- the shared label recognizer of RFC-BPR-0047,
  [BPR-1076](../issues/60-done/BPR-1076-implement-the-complete-catalog-and-bounded-lossless-prefix-recognizer.md)
  (Done), through which RC55-03 is judged;
- the shared label authoring controls of RFC-BPR-0048,
  [BPR-1082](../issues/60-done/BPR-1082-compose-convention-edits-through-shared-durable-draft-services.md)
  and
  [BPR-1083](../issues/60-done/BPR-1083-implement-selected-authoring-controls-and-shared-label-visuals.md)
  (Done);
- the native Markdown reader of RFC-BPR-0036 (implementation accepted;
  [BPR-0917](../issues/60-done/BPR-0917-close-rfc-0036.md) is Done).

The front matter's `depends_on` keeps RFC-BPR-0053 and 0054, and
`owner_dependency_amendment` names the three removed closure dependencies and
what is used from them. This amendment changes no RC55 criterion and closes
none of RFC-BPR-0036, 0047 or 0048: each stays open on its own cards, and
RFC-BPR-0036's status-only close remains separate. The question and its answer
are recorded on
[BPR-1188](../issues/60-done/BPR-1188-close-rfc-0055.md) and in the
[execution ledger](../verification/RFC-BPR-0053-0061.md).

## Normative contracts

The detailed contracts [FEEDBACK.md](assets/bpr-review-cycles-v1/specs/FEEDBACK.md) are normative parts of this proposal. The common [domain](assets/bpr-review-cycles-v1/specs/DOMAIN.md), [decisions](assets/bpr-review-cycles-v1/supporting/DECISIONS.md) and [security boundary](assets/bpr-review-cycles-v1/specs/SECURITY-AND-PACKAGING.md) apply. If a live predecessor differs, record a specific compatibility decision; do not quietly implement a weaker alternate workflow.

## Acceptance criteria

- **RC55-01:** Author range/file/module/cycle notes and one multi-location discussion from full source or a diff without mandatory per-item metadata.
- **RC55-02:** Original source and ranges remain immutable; current exact/ambiguous/deleted/unavailable correspondence is separately represented.
- **RC55-03:** Plain and all twelve shared labels preserve authored Markdown and do not imply patches, severity, permissions or verification.
- **RC55-04:** Suggestion authoring preserves original/replacement bytes, insertion/deletion, explicit intent, byte ranges and before/after preview; an in-place edit of a file becomes one exact suggestion per separate changed run of lines (owner amendment, 2026-10-01).
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

See the [integration map](assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md), [acceptance matrix](assets/bpr-review-cycles-v1/verification/ACCEPTANCE-MATRIX.md) and [product verification contract](assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md).
