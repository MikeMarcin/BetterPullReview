---
schema: "implement-rfc.card.v1"
id: "BPR-2363"
title: "Complete skill names in the item side chat"
status: "Ready"
rfcs: []
depends_on: ["BPR-1916", "BPR-2322"]
priority: "P2"
metadata: {"kind":"defect","origin":"owner, 2026-11-23","reservation":"docs/issues/_reservations/BPR-2363--BPR-2363--f17be8200e59.json"}
---

# BPR-2363: Complete skill names in the item side chat

> The side chat composer isn't doing skill completion popups like the agent tab composer does

The item side chat opens beside a review report or beside the code when a
reviewer asks about a finding, draft or thread. Its composer should offer
the Agent tab's skill picker.

## Acceptance

- AC1. Typing a slash and part of a skill name in the item side chat shows
  matches from the same catalog as the Agent tab, with their sources.
- AC2. The picker inserts the chosen skill into the draft and leaves the
  keyboard in the composer. Return chooses while the picker is open and
  sends the question when it is closed. Shift+Return still inserts a new line
  with the picker closed.
- AC3. The existing follow-up course exercises the composed side chat with
  fixture skills and the fixture agent. Sending a completed name uses that
  skill's inline guidance in the item's ordinary Ask turn.

## Plan

Use the Agent tab's picker and catalog as-is in the item side chat. Resolve
skill references in the typed question at the existing Ask boundary, and
attach the named guidance to its tool-free request using the existing
inline fields. Check the real report's side chat with keyboard input and
the fixture agent.
