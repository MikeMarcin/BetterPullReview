---
schema: "implement-rfc.card.v1"
id: "BPR-1515"
title: "Add a keyboard shortcut for Accept File"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Need a hotkey for accept file","related_cards":["BPR-1507"],"milestone_plan":{"id":"bpr-1515-accept-file-hotkey","path":"docs/rfcs/plans/bpr-1515-accept-file-hotkey/milestone-plan.json"}}
---

# BPR-1515: Add a keyboard shortcut for Accept File

Provide a discoverable keyboard shortcut for accepting the current file. Reuse the same semantic action and eligibility as the file Accept control. Choose a shortcut consistent with existing platform conventions after checking collisions.

## Acceptance

- AC1: An eligible current file can be Accepted with a keyboard shortcut, without opening the file action menu.
- AC2: The shortcut is discoverable in the existing action/menu or shortcut presentation and does not collide with existing bindings or steal normal text editing input.
- AC3: No-current-file, excluded/unavailable, stale authority and other ineligible states follow the existing Accept action policy. Repeated presses must not inadvertently revoke acceptance unless the established shortcut contract explicitly calls for a toggle.
- AC4: Verify actual key delivery through the composed window, matching current file/epoch acceptance and focused shortcut collision/eligibility cases.

## Coordination

BPR-1507 owns the one-click Accept control; assign this related card to its implementer and share the canonical action. Keep card acceptance/evidence distinct. Load frontend-design:frontend-design before any UX edits per AGENTS.md. Never operate or capture the owner’s live desktop.
