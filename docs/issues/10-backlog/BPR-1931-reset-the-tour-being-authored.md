---
schema: "implement-rfc.card.v1"
id: "BPR-1931"
title: "The author can reset or clear the tour they are authoring"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-07, coordinator chat: 'I want the ability to reset/clear the tour that I'm authoring'","related_cards":["BPR-1900","BPR-1925"]}
acceptance: []
---

# BPR-1931: The author can reset or clear the tour they are authoring

## Change

- The Tour tab offers **Reset tour**, which clears Problem, Solution, every
  stop, any pending proposal, and the tour's chats and focus. It confirms
  with a plain sentence that names what is lost. It can be undone, at least
  for the session.
- It never touches a published tour. If the tour was published, say that
  resetting affects only the local draft.
- Persistence goes through the host. The widget performs no effects.

## Acceptance

- AC1. Reset clears the local tour draft, proposals, chats and focus. Undo
  restores them exactly (Qt and host tests).
- AC2. A published tour on GitHub is untouched (a test with a fixture).
- AC3. Owner look: may share a session with the whole-tour feedback card.
- AC4. The documentation gate passes.
