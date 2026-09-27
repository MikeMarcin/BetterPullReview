---
schema: "implement-rfc.card.v1"
id: "BPR-1221"
title: "Show tour stop status before the number"
status: "In Progress"
rfcs: ["RFC-BPR-0039"]
depends_on: ["BPR-0995"]
metadata: {"milestone":"qualification-closeout","milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"},"owner_amendment":"2026-09-27 hand-back 90: replace trailing em-dash Editing with a compact icon to the left of the number"}
---

# BPR-1221: Show tour stop status before the number

The owner requested a follow-up after accepting r3's ordinary layout: replace
the trailing status text with a legible status icon before the stop number.
Keep the six-dot drag handle separate and preserve the core preparation state.
This independently eligible presentation slice belongs to qualification-closeout,
mandate `r1-20260926-land-each-milestone`.

## Acceptance criteria

- **AC1:** Suggested, Editing, Ready and Needs attention have distinct compact
  shapes before the stop number; the title has no appended preparation label.
- **AC2:** Every row exposes its complete ordinal, title and preparation state
  through its accessible name and hover tooltip. Status cannot depend on color.
- **AC3:** The real composed native course verifies selected/unselected rendering,
  high contrast and larger scale/font legibility, while the separate drag handle,
  selection, keyboard reordering and local editing retain their behavior.
- **AC4:** The owner judges the revised row affordance in the affected
  ordinary-layout attended candidate. Existing accepts stay bound to their exact
  source/binary; this card remains human-pending until that verdict.

## Scope and evidence

Qt presentation and the existing public composition course only. No core state,
provider authority, persistence format or remote effects change. Keyboard feedback
correction remains BPR-1013's existing AC39-06 obligation.
