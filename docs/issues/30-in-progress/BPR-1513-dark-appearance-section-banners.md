---
schema: "implement-rfc.card.v1"
id: "BPR-1513"
title: "Reduce bright Appearance section banners in dark mode"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"I don't like the dark mode section banner background color in appearance, it's like white on white and a flash bang","milestone_plan":{"id":"bpr1513-dark-appearance-banners","path":"docs/rfcs/plans/bpr1513-dark-appearance-banners/milestone-plan.json"}}
---

# BPR-1513: Reduce bright Appearance section banners in dark mode

Correct the overly bright section banner backgrounds in Appearance when using dark mode. Use a subdued dark-theme surface with legible text that fits the surrounding settings UI.

## Acceptance

- AC1: Appearance section banners in dark mode no longer present the bright white-on-white/flash effect reported by the owner.
- AC2: Banner text remains clearly legible with restrained surface contrast, using the existing theme system rather than disconnected hard-coded styling.
- AC3: Light mode and supported theme switching remain correct, including switching while Appearance is open.
- AC4: Inspect actual offscreen renders of Appearance in dark and light modes and exercise the theme-switch path; prepare the required owner visual judgement with an exact frozen candidate.

## Coordination

Load frontend-design:frontend-design before UX edits as required by AGENTS.md. Coordinate Appearance/settings paths with BPR-1512 (center ribbon width setting). Never operate or capture the owner’s live desktop. Any human judgement uses a short attended session and frozen build.
