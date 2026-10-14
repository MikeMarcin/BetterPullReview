---
schema: "implement-rfc.card.v1"
id: "BPR-1511"
title: "Scope unread comments badge to the current file"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Badge for unread comments should only show when there is one on the current file","milestone_plan":{"id":"bpr1511-current-file-unread-badge","path":"docs/rfcs/plans/bpr1511-current-file-unread-badge/milestone-plan.json"}}
---

# BPR-1511: Scope unread comments badge to the current file

Show the file-context unread-comments badge only when the currently selected file has an unread comment. Unread comments on other files must not make this badge appear.

## Acceptance

- AC1: The badge appears when the current file has unread comments and is absent when that file has none, even when another file has unread comments.
- AC2: Any count shown by this badge reflects only the current file’s unread comments, under the existing unread policy.
- AC3: File changes, marking comments read, refresh and authority replacement update the badge correctly without stale counts or cross-file leakage. With no current file the badge is absent.
- AC4: Verify the composed file-selection and unread-update paths with representative multiple-file comments and an offscreen rendered state.

## Coordination

Identify the existing file-context badge before changing it; retain distinct PR-wide indicators and existing unread semantics. Load frontend-design:frontend-design before UX edits as required by AGENTS.md. Coordinate shared tab/comment projection paths with BPR-1509 and BPR-1510. Never operate or capture the owner’s live desktop. Required visual judgement goes through a short attended session and frozen candidate.
