---
schema: "implement-rfc.card.v1"
id: "DRAFT-0018"
title: "Native pane Find controls, decorations and exact reveal"
status: "Backlog"
rfcs: ["RFC-DRAFT-0015"]
depends_on: ["DRAFT-0016"]
metadata: {"milestone": "pane-find", "provisional": true}
---

# DRAFT-0018 — Native pane Find controls, decorations and exact reveal

## Acceptance

- **AC1:** AC01 and AC02 pass through the shipped focused split/unified source reader, including offscreen unchanged source, selection/caret seeding, multiline options, fixed selection, Unicode/line-ending exact selection, invalid regex and wrap/focus.
- **AC2:** View > Find command availability, editor/modality ownership and native layouts use the required frontend-design skill; visible highlights and scroll-track markers do not alter source/diff geometry.
- **AC3:** Match traversal/reveal through the shared controller performs no rescan/recompile/read/diff; palette, accessible counts/status and narrow-width rendering are verified.
