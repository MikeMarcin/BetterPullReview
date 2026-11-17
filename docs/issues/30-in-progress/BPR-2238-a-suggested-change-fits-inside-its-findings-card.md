---
schema: "implement-rfc.card.v1"
id: "BPR-2238"
title: "A suggested change fits inside its finding's card"
status: "In Progress"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2236--BPR-2238--777d083d72cc.json","origin":"BPR-2219's look review-report-v002, 2026-11-16"}
---

# A suggested change fits inside its finding's card

## Origin

In the report's Cards look ([[BPR-2219]], docs/design/review-report-v2) the suggested-change box laid over its place in the page runs a few points past the right padding of its finding's bordered box, in light and dark.

## Scope and acceptance

- **AC-a.** A code box and a suggested change stand within the finding's box, with the box's padding at both sides, at every pane width.
- **AC-b.** `qt_agent_report_view` measures it.

## Cause

`agent_report_view`'s code place took a finding's cell to be the page less
the plain look's rule and left padding (3 + 16 points). The cards look's box
has a border at each side and 14 points of padding at both, so a box laid
over its place ran 14 points into the right padding, to one point from the
border.

## Delivered

`c3f52b4ca`. The place's width is read from its cell: the table's borders,
the fixed rule column, the cell's own padding at both sides and any cell
spacing. Placing the box, its width is held to the right of the line the
layout gave its place.

## Acceptance

| Criterion | Check | Result |
|---|---|---|
| AC-a | `qt_agent_report_view`: a finding with a code block and a suggested change, cards look, panes 360, 520, 700, 900 and 1400 wide; each box's inset from the finding box's laid-out frame | Pass at `c3f52b4ca`: left 18 (border, rule, padding), right 16 (padding, border) at every width. The same check on the code before: right 1 at 360, failing |
| AC-b | The same course | Pass |

Shown to the owner in the batched look of BPR-2232, BPR-2231, BPR-2235,
BPR-2237 and this card.
