---
schema: "implement-rfc.card.v1"
id: "BPR-2238"
title: "A suggested change fits inside its finding's card"
status: "Done"
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

## Owner look (filed, not waited on)

Attended-review session `6e525e076ba6400a9dbc677a2c9bbd88`, state directory
`~/.attended-review-state/review-followups-v001-2026-11-16/` (URL in its
`serve.log` only, host pid in `serve.pid`), served on the tailnet. Question
`suggestion-in-card` (Accept or Reject) on the frozen build `review-followups-v001` (root
`~/review-followups-v001.pn_cb0u7`, from `0feba206bd70`, Fixture agent
only, the freeze's launcher check passed). Lifecycle: answered and closed. A Reject
reopens the work as a new card.

### Verdict, look review-followups-v001 (handed back 2026-11-16)

- **Accept** `suggestion-in-card` (decision `37fb2d60`), no note. Session
  `6e525e076ba6400a9dbc677a2c9bbd88` handed back all three questions
  (`edited-prompt-shows-text`, `suggestion-in-card`, `tour-chat-prompt-turn`)
  accepted at 2026-11-17T06:15:10Z. Its host is stopped and its root
  `~/review-followups-v001.pn_cb0u7` removed with the freeze tool.
