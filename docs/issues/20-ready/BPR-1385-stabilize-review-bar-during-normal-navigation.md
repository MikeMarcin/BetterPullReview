---
schema: "implement-rfc.card.v1"
id: "BPR-1385"
title: "Stabilize the review bar during normal navigation"
status: "Ready"
rfcs: ["RFC-BPR-0005", "RFC-BPR-0052"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"corrective","origin":"Owner report and toolbar screenshot, 2026-10-10","reservation":"docs/issues/_reservations/BPR-1385--BPR-1385--e781f12baaa1.json","related_cards":["BPR-1129","BPR-1130","BPR-1255"],"milestone_plan":{"id":"bpr1385-review-bar-stability","path":"docs/rfcs/plans/bpr1385-review-bar-stability/milestone-plan.json"}}
---

# Stabilize the review bar during normal navigation

## Mandate and diagnosis

The owner asks for stability while working without resizing the window. The
highlighted comparison/status menu expands and shrinks frequently; file and change
navigation adds smaller shifts. File and work a corrective card directly.

The comparison button currently sizes itself from readiness and comparison text.
The file and change labels also use their current text's natural width. Both move
the adjacent arrow hit targets even though the available window width is unchanged.

## Acceptance

- AC1: At a fixed window size, comparison readiness, failure, recovery and named-comparison text changes preserve the comparison button and downstream navigation geometry. Full text remains available through tooltip and accessibility, and the comparison menu remains usable.
- AC2: File and change navigation, including unselected/loading states, differing glyph widths and digit boundaries, preserves arrow hit targets without hiding the current position at widths that have room for it.
- AC3: Actual window resizing and font/style changes still adapt the row, preserve the supported minimum width and keep primary actions accessible. No effects or semantic policy move into Qt.
- AC4: A regression fails before the fix, passes afterward, and exercises the rendered workbench and a composed application path. Focused checks, touched-file formatting and the documentation gate pass.

## Design and scope

Use the loaded frontend-design skill with the existing native palette, typography,
focus treatment and control order. Reserve stable comparison and position slots;
shorten visible text to its allocated space while keeping complete descriptions.
Width changes follow available space and appearance, not the current sentence or
ordinal. This is a measurable layout correction; no new subjective design approval
is introduced. Existing RFC-wide native and human obligations remain unchanged.

## Evidence

Pending implementation and direct geometry checks.
