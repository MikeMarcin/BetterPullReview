---
schema: "implement-rfc.card.v1"
id: "BPR-0676"
title: "The epoch line does not show whether it is working"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
---

# The epoch line does not show whether it is working

The owner, 2026-09-07: "The epoch line is quite hard to see if it's working
properly or not."

Their capture shows the line as the word "Updates", a pair of small marks,
and "Sep 7, latest", above a "Compari…hole PR" control truncated to
illegibility. RFC-BPR-0028 AC-02 asks for a node per upload with labels
beneath, bracket handles, a blue span and an amber tick for the last
review. At this width almost none of that is legible, and nothing says
whether the two anchors are where the reviewer put them or where the
product defaulted.

## Scope

- The line states its own state in words: which two uploads are being
  compared and whether that is the default or a choice the reviewer made.
- The nodes, the span and the amber tick are legible at the widths the
  workbench supports, or the line says it is too narrow and offers the
  detail another way.
- Owned paths: `src/adapters/qt` epoch comparison control, its tests and
  the render evidence they pin.

## Acceptance

- At 1280 and 1920 wide the line names the compared pair in words and its
  marks are legible; the owner accepts it in an attended session.

## Notes (2026-09-07)

- **What was missing.** The line drew nodes and a named comparison and
  said nothing about whether a pair had actually resolved. The sentence
  that says it, "Exact epoch pair. Left available; right available. Ready
  for diff.", existed already but lived in the details region, behind a
  disclosure the owner had no reason to open.
- **What it says now.** Three words on the line itself, from the same
  facts and written in the same place as that sentence so the two cannot
  disagree: "no pair yet" before anything resolves, "pair ready" when the
  pair can be read, "pair not readable" when it is retained but cannot.
  The long sentence is its tooltip.
- **What is not done.** The card also asks whether the pair is the default
  or a choice the reviewer made, and for the nodes and the amber tick to
  be legible at every width. The first is knowable from the selection and
  is worth adding once the owner has seen whether the summary is enough;
  the second was mostly the drawers, which
  [[BPR-0674]] now collapses in Overview, and the rest is a judgement to
  make against a real window rather than a test.
- **Evidence.** `qt_epoch_comparison_control_scale_{100,125,200}` pin all
  three states and the tooltip. Fifty-six Qt and composition selectors
  pass, with `check_boundaries`, strict tidy, `clang-format --Werror` and
  `verify_documentation.py`.
- **Negative mutations.** A summary that always says ready fails "the line
  says when nothing is being compared yet"; a summary that carries no
  detail fails "the line says a readable pair is compared, and carries the
  detail".
