---
schema: "implement-rfc.card.v1"
id: "BPR-1505"
title: "Place Home Cancel left of Changes to preserve action alignment"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Put cancel on the left of changes etc on home so it doesn't move the alignment.","milestone_plan":{"id":"bpr1505-home-cancel","path":"docs/rfcs/plans/bpr1505-home-cancel/milestone-plan.json"}}
---

# BPR-1505: Place Home Cancel left of Changes to preserve action alignment

Move Home's Cancel control to the left of Changes and the adjacent actions so showing or hiding Cancel does not move their alignment. Preserve the existing cancellation behavior.

## Acceptance

- AC1: Cancel appears to the left of Changes and the adjacent Home actions.
- AC2: Showing and hiding Cancel leaves the other actions aligned in the same positions.
- AC3: The existing cancellation action continues to work; verify the rendered Home state offscreen and exercise the relevant interaction path.

## Implementation

Load frontend-design:frontend-design before UX work, as required by AGENTS.md. Keep scope to this Home layout correction. Do not capture or operate the owner's live desktop.
