---
schema: "implement-rfc.card.v1"
id: "BPR-1512"
title: "Make center ribbon width configurable and halve its default"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Make the center ribbon width a setting and make the default half the current width"}
---

# BPR-1512: Make center ribbon width configurable and halve its default

Expose the center ribbon width through the existing settings workflow. Set the default to half the current default width. Inspect the actual center ribbon implementation and measure its existing width before choosing the new value.

## Acceptance

- AC1: The center ribbon width is an accessible user setting with sensible supported bounds and a clear unit/label consistent with existing settings.
- AC2: A new/default configuration uses half the previous default center ribbon width. Reset to default selects this new value; preserve existing explicit user choices if the setting already exists in some form.
- AC3: Changing the setting updates the actual center ribbon and persists through the established settings lifecycle and restart.
- AC4: The narrower default and supported widths preserve usable ribbon interaction, adjacent panes and rendering. Verify the real offscreen workbench/settings path and focused persistence/default checks.

## Implementation

Load frontend-design:frontend-design before UX work as required by AGENTS.md. Follow core policy, host effects and Qt presentation boundaries. Coordinate any shared workbench/settings paths with other active testing-note cards. Never drive or capture the owner’s live desktop. Required visual judgement uses a short attended session and exact frozen candidate.
