---
schema: "implement-rfc.card.v1"
id: "BPR-1218"
title: "Preview and export frozen diagnostic selections"
status: "Backlog"
rfcs: ["RFC-BPR-0027"]
depends_on: ["BPR-1217"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0027-advanced","path":"docs/rfcs/plans/rfc-bpr-0027-advanced/milestone-plan.json"}}
---

# Preview and export frozen diagnostic selections

Export exact previewed scopes with fresh anonymized joins or explicit identifying detail, preserving useful sanitized diagnostics and originals, refusing or counting malformed records without network effects.

## Acceptance

- AC1: Freeze and reprocess every selected field, preserve joins, disclose capture-time omissions and keep originals intact.
- AC2: Verify malformed/mixed-mode/session/ring selection, local-only writes and negative content cases through public UI.

Mandate r2 (2026-09-26): implement the full advanced phase and land each accepted milestone on main. No live sign-in or PR mutation is authorized.
