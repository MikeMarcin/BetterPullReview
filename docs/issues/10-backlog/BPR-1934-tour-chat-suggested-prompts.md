---
schema: "implement-rfc.card.v1"
id: "BPR-1934"
title: "Tour chats offer suggested prompts, as the Agent tab does"
status: "Backlog"
rfcs: []
depends_on: ["BPR-1925", "BPR-1930"]
priority: "P2"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-07: 'Tour side chat should get suggestions like the agent tab has'","related_cards":["BPR-1877","BPR-1925","BPR-1930"]}
acceptance: []
---

# BPR-1934: Tour chats offer suggested prompts, as the Agent tab does

## Change

Load `frontend-design:frontend-design` first.

- An empty tour chat shows suggested prompts in the Agent tab's style and
  component (BPR-1877). This covers each part's chat (Problem, Solution, a
  stop) and the whole-tour chat (BPR-1930). One click sends a prompt, and
  they disappear once the chat has turns. Reuse the Agent tab's suggestion
  component; don't build a second one.
- The prompts fit the chat's scope. Examples:
  - **A stop:** "What does this code do?", "Why does this change matter?",
    "Rewrite this explanation more simply", "Is this the right place for this
    stop?".
  - **Problem or Solution:** "Make this clearer", "What's missing?",
    "Shorter".
  - **The whole tour:** "Too many stops?", "Suggest a better order",
    "Focus on …", "What's not covered?".
  
  Keep the prompt lists in one place in the app layer, not in widgets, so
  Agent settings can later make them editable.
- They are offered only when the agent is set up (BPR-1881's gate), in the
  agent accent.

## Acceptance

- AC1. Each tour chat kind, when empty, shows its prompts. A click sends it,
  and the prompts hide after the first turn (Qt test).
- AC2. Reuses the Agent tab's suggestion component.
- AC3. Owner look: Accept/Reject on a frozen build.
- AC4. The documentation gate passes.
