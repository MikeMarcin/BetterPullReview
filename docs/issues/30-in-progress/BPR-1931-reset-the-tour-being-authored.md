---
schema: "implement-rfc.card.v1"
id: "BPR-1931"
title: "The author can reset or clear the tour they are authoring"
status: "In Progress"
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

## Implementation v001 (2026-11-07)

`frontend-design:frontend-design` was loaded first. Implemented with
[[BPR-1930]] on branch `worktree-agent-a2168bac3b72f3c50`. RFC-BPR-0092
Amendment BPR-1930, BPR-1931 records it.

- **Core**:
  - `reset_tour` empties the draft. Its identity, head and published binding
    stay, and a published stop is retired as Remove retires it.
  - `reset_tally_of` counts what a reset would clear.
- **Composition** (`window_tours.cpp`):
  - `reset_tour` restores the empty draft through the tour host, as one
    edit, and keeps the draft, session (chats, focus) and proposal it
    cleared;
  - `undo_tour_reset` restores all of them while the draft's revision is the
    reset's, and Undo reset goes once the tour changes;
  - no GitHub request is made, and nothing is reset while the agent runs.
- **Qt**: Reset tour is a quiet action in the tour's header.
  - It asks first in one sentence naming what is lost; for a published tour
    it says the tour on GitHub stays as it is.
  - Keep the tour or Reset tour answer the question.
  - Undo reset is offered after a reset.
  - The widget only dispatches.
