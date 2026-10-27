---
schema: "implement-rfc.card.v1"
id: "BPR-1773"
title: "Ask the agent about a conversation: review threads and private notes, with the code they are on"
status: "In Progress"
rfcs: ["RFC-BPR-0094"]
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1772--BPR-1773--b27f9d2285df.json","kind":"feature","found_by":"owner chat, 2026-10-26","authoring":"frontend-design:frontend-design loaded for the Qt surfaces","related_cards":["BPR-1594","BPR-1753","BPR-1245"]}
acceptance: []
---

# BPR-1773: Ask the agent about a conversation

Implements [RFC-BPR-0094](../../rfcs/RFC-BPR-0094-ask-the-agent-about-a-conversation.md),
whose problem, owner decisions and criteria this card does not repeat.

## Slices

- Core: the entry point and the bounded conversation (`686c8c448`).
- Application, composition and Qt: threads, notes, the chip, Use as reply.

## Evidence

Recorded per criterion once the focused tests pass.
