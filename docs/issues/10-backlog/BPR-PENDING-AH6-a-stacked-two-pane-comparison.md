---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AH6"
title: "A stacked two-pane comparison, base over head"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0090 adoption, 2026-10-21 (owner answers in session e5387830d18edd8d541b96e47ecbd9cb)","related_cards":["BPR-1723"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0090's design frames v2"}
---

# BPR-PENDING-AH6: A stacked two-pane comparison, base over head

## Origin

The owner's note on RFC-BPR-0090's Q4 (2026-10-21, session
`e5387830d18edd8d541b96e47ecbd9cb`), verbatim: "I kind of want to say 2 pane
with top over bottom but I think that's too big of a change to land here."
Q4 itself was answered A: in one pane, Suggest change shows the head file
while editing.

## Proposal to shape

A layout with the base pane above the head pane, each full width, as a third
presentation beside split and one pane. It needs its own design: how the
link map and the shared scroll work vertically, where the drawer sits, and
whether Suggest change edits the lower pane as RFC-BPR-0090 edits the right
one.

## Acceptance criteria

- AC1: An RFC or owner decision shapes the layout before any code.
