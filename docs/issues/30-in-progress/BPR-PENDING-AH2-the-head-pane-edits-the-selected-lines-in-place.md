---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AH2"
title: "The head pane edits the selected lines in place with the shared editor"
status: "In Progress"
rfcs: ["RFC-BPR-0090"]
depends_on: ["BPR-PENDING-AH1"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0090 adoption, 2026-10-21 (owner answers in session e5387830d18edd8d541b96e47ecbd9cb)","related_cards":["BPR-1723"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0090's design frames v2","milestone":"RFC-BPR-0090 M2"}
---

# BPR-PENDING-AH2: The head pane edits the selected lines in place with the shared editor

RFC-BPR-0090 M2 ([RFC](../../rfcs/RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md#milestones)). The shared editor of RC55-07 over
the head pane at the same geometry, reused from Review Cycles.

## Scope

- `qt::suggestion_editor` gains the editable span, an external scroll offset
  so it follows the comparison, a number gutter and row fills, and keeps its
  behaviour when none is set.
- `diff_workbench` puts it over the head pane (split) or shows the head file
  alone (one pane), dims ribbons that touch or follow the range, and ends it.
- Every Suggest change entry (Shift+Return, the code menu, the review bar)
  opens it after `decide_suggestion_range`.

## Acceptance criteria

- AC1: RFC AC01: from each entry, on a selection or the caret, the head pane
  becomes editable with unchanged text, scroll, numbers and colours and the
  selection kept; refused entries say why.
- AC2: RFC AC02: RC55-07's gestures work inside the range on the composed
  window, an edit to a fixed byte is refused with the status line's words,
  Enter grows the range and emptying it is a deletion.
- AC3: Qt tests run offscreen; format, tidy on the changed files, the
  command-correctness reseal and the documentation gate pass.
