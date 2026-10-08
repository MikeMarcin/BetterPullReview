---
schema: "implement-rfc.card.v1"
id: "BPR-1337"
title: "Beautify the top section of Discuss"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","found_by":"Owner chat, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1337--BPR-1337--2dee31ef43be.json","requested_skill":"frontend-design","related_cards":["BPR-1212","BPR-1089","BPR-1083","BPR-1290","BPR-1293","BPR-1294"]}
---

# BPR-1337: Beautify the top section of Discuss

## Owner note

Owner, chat, 2026-10-08, verbatim: "The top section of discuss could use some
beautification".

## Scope

Discuss's top section (`github_conversation_panel`, shown while the panel is
Discuss's page): the threads summary, the scope (whole pull request or one
file), the file choice, the Unread and Unresolved filters, Clear, and an
Overview count's drill-down caption with its × ([[BPR-1089]]). It sits above
the conversations Discuss lists; since [[BPR-1212]] slice 2 it also sits above
the narrowed list beside the full composer and the ⌥R summary.

Designed with `frontend-design`, inside the application's language: minimal
words, icons with tooltips, [[BPR-1083]]'s marks, palette roles only, and the
chip language Conversations' bar already uses ([[BPR-1293]]).

- Every control stays, with an accessible name and description, in the same
  tab order.
- Discuss's behaviour and keys stay: scope, file, filters, Clear, the
  drill-down and its ×, the summary's counts, and Clear's reset of the list.
- Beside the code nothing changes: the drawer's two filter icons stay on its
  tab row.

## Acceptance

- `BPR-1337:inventory`: the card records the before renders (1440x900, light
  and dark, on a fixture with conversations, pull request comments and review
  summaries) and what in the top section looks unpolished.
- `BPR-1337:candidates`: two candidates as offscreen renders, the better one
  chosen with a written rationale.
- `BPR-1337:built`: the chosen candidate is built in Qt; tests cover its
  structure, every control's accessible name and both themes, and a control
  fails against the code before the change where behaviour changes.
- `BPR-1337:owner-look`: the owner, in one short attended session on a frozen
  build, picks between the two candidates' renders and accepts or rejects the
  built one. Pending until answered.
