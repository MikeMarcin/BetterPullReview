---
schema: "implement-rfc.card.v1"
id: "BPR-2235"
title: "A tour chat's prompt turn shows the prompt's title"
status: "Backlog"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2231--BPR-2235--072fd709068e.json","origin":"BPR-2219's known gaps, 2026-11-16"}
---

# A tour chat's prompt turn shows the prompt's title

## Origin

In the Agent tab a turn sent from a prompt shows the prompt's name, with Show prompt for what was sent ([[BPR-2219]], AC-h; owner, 2026-11-16: "All of this extra context from a prompt we select should be hidden.. it's just noise"). The tour chats (a stop, Problem, Solution, the whole tour) still show a named prompt's whole text.

## Scope and acceptance

- **AC-a.** A tour chat's turn sent from a prompt shows the prompt's name as the user's turn.
- **AC-b.** What was sent is behind the same small disclosure the Agent tab uses.
- **AC-c.** A typed message shows as typed.
