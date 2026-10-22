---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AH1"
title: "Core decides a selected-lines suggestion: range, editable span, plan, body, rebuild, convert and carry"
status: "In Progress"
rfcs: ["RFC-BPR-0090"]
depends_on: ["BPR-1723"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0090 adoption, 2026-10-21 (owner answers in session e5387830d18edd8d541b96e47ecbd9cb)","related_cards":["BPR-1723"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0090's design frames v2","milestone":"RFC-BPR-0090 M1"}
---

# BPR-PENDING-AH1: Core decides a selected-lines suggestion: range, editable span, plan, body, rebuild, convert and carry

RFC-BPR-0090 M1 ([RFC](../../rfcs/RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md#milestones)). The core decisions, as pure functions
of values the host already holds, with core tests that run under `core-only`.

## Scope

- `decide_suggestion_range`: head side, one changed region (a single line
  too), and no range on a filtered diff whose right side is not head.
- The editable span in `suggestion_text_editor`: edits that would change a
  fixed byte are refused whole, selections into fixed text are clipped,
  the range grows and shrinks with line breaks, and its extent is part of
  undo.
- `plan_pull_request_suggestion`: changed or not, anchor, block, composed
  body, what it changes, and placeable or the reason.
- `split_suggestion_body`, `rebuild_suggestion_span`,
  `convert_suggestion_to_text` and `decide_suggestion_carry`.

## Acceptance criteria

- AC1: Each decision above has core tests covering its refusals, including
  the fixed-byte refusals at both ends of the span, growth, deletion, a block
  holding backticks, an endings-only edit, and a composed body whose label
  header edit changes only the header's bytes (RFC AC03 and AC05, core share).
- AC2: A kept body rebuilds the span exactly, and an unparsable block, a
  second block and another comparison are refused (RFC AC06, core share).
- AC3: Carry is decided from the old-to-new head correspondence exactly
  (RFC AC08, decision), and a filtered line is placeable only when it is
  unchanged at head (RFC AC09).
- AC4: `core-only` builds and its tests pass; format, tidy on the changed
  files and the documentation gate pass.
