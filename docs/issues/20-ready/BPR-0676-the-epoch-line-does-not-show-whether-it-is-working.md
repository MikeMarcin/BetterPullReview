---
schema: "implement-rfc.card.v1"
id: "BPR-0676"
title: "The epoch line does not show whether it is working"
status: "Ready"
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
