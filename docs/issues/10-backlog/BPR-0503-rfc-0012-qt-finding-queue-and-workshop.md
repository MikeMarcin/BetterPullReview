---
id: BPR-0503
title: "Qt: finding queue and before/after comment workshop"
status: Backlog
rfc: RFC-BPR-0012
depends_on: [BPR-0498, BPR-0500]
---

# Qt: finding queue and before/after comment workshop

A finding queue presents delivered findings with their anchor state and offers
exactly the six actions `docs/ux/ai-experience.md` names: open evidence, ask
follow-up, convert to local draft comment, convert to local draft defect, add
to private notes, and dismiss. The workshop lives inside
`src/adapters/qt/include/bpr/qt/review_draft_editor.hpp`, keeps the reviewer's
original draft intact, and shows a proposed revision beside it. Both obey the
editor's existing rule: the only way out of this widget is a semantic command.

## Required final state

- The queue shows severity, confidence, category, claim, evidence,
  consequence, and the anchor state, so a reviewer sees that a finding did not
  land before acting on it.
- Each of the six actions emits one intent naming the exact finding. The
  widget performs no promotion itself and reaches no store.
- The workshop displays the original and the proposed revision at the same
  time. Accepting emits one `review_draft_command`; rejecting leaves the
  original byte-identical.
- Neither widget holds a provider, endpoint, store, or process member.
- Surface decision, recorded rather than assumed: no Markdown rendering
  surface exists anywhere in the tree, so the before/after view is plain text
  in the editor's existing presentation. Adding a renderer to the Qt adapter
  for one surface is the cost the implementing slice would have to justify in
  the ledger if it decides otherwise.

## Non-goals

- The one-command promotion rule and its refusals — BPR-0498.
- Chat, chips, and the stale banner — BPR-0502.
- Decode, bounds, and delivery — BPR-0500.
- Publishing — RFC-BPR-0008's controls, unchanged by this card.

## Required direct evidence

- A new `qt_finding_queue_view` registered in
  `tests/adapters_qt/CMakeLists.txt` beside `qt_review_draft_editor`, with the
  workshop cases extending the existing `qt_review_draft_editor` suite; run
  `ctest --preset dev -R '^(qt_finding_queue_view|qt_review_draft_editor)$'
  --output-on-failure`.
- A table-driven case over all six actions asserts one intent each carrying
  the exact finding identity; a seventh action added without a case fails the
  count.
- Accepting a workshop revision emits exactly one `review_draft_command` and
  performs no store; rejecting leaves the original compared byte for byte.
- Both texts are asserted on two separately presented values, so a single
  buffer that overwrote the original fails.
- A field-reading guard fails to compile when a provider, endpoint, or store
  member is added to either widget.
- Each assertion is shown to fail when the behaviour it constrains is
  neutered.
