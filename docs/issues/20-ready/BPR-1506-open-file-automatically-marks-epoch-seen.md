---
schema: "implement-rfc.card.v1"
id: "BPR-1506"
title: "Automatically mark an opened file Seen for its epoch"
status: "Ready"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Seen should be automatically applied to a viewed file on an epoch when you open it"}
---

# BPR-1506: Automatically mark an opened file Seen for its epoch

Opening a file in an epoch automatically applies Seen to that viewed file in that epoch. Reuse the existing receipt policy and durable workflow, with the core deciding, the host performing and Qt presenting.

## Acceptance

- AC1: Opening and successfully presenting a file in an epoch automatically records and projects Seen without a separate gesture.
- AC2: Seen binds to the exact viewed file and epoch; opening one cannot mark another file or epoch, and stale or failed loading cannot mark unseen content.
- AC3: Automatic Seen follows existing persistence and reload behavior and remains distinct from any stronger reviewed/verified state.
- AC4: Exercise the composed public open-file path and focused identity, stale-result and persistence checks.

## Implementation

Inspect the existing meaning and acceptance point for Seen before changing policy. Owner explicitly requests this behavior. Keep source ownership separate from BPR-1505's Home action layout. If visible UX changes are needed, load frontend-design:frontend-design first as required by AGENTS.md. Never operate or capture the owner's live desktop.
