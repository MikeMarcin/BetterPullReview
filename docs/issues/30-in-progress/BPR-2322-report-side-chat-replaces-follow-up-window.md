---
schema: "implement-rfc.card.v1"
id: "BPR-2322"
title: "The report tab's side chat replaces the follow-up window"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux-refactor","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2322--BPR-2322--b934dff5c89b.json","origin":"owner, 2026-11-20","related_cards":["BPR-2286","BPR-1925","BPR-1995","BPR-1914"],"related_rfcs":["RFC-BPR-0128","RFC-BPR-0100","RFC-BPR-0101"]}
---

# BPR-2322: The report tab's side chat replaces the follow-up window

## Owner (2026-11-20)

"I just finally saw the report ask follow-up. It looks like it implements
another completely new agent interface. I think we should instead make it use
something akin to the side chat from the tour onto the report tab."

## Change

[[BPR-2286]] opened a separate follow-up window with a look of its own for Ask
follow-up (a report finding, an Agent tab finding card) and Ask about this (the
draft composer, a thread's Ask menu). That window goes. In its place a side
chat column, built from the Tour tab's side chat parts ([[BPR-1925]],
[[BPR-1995]]) and wearing its look, opens beside the report (and beside the
code for a draft or a thread): the seam in the agent accent, the item it is
about at the top with Copy chat and Close, the reviewer's words in bubbles on
the right, the agent's replies as Markdown on the left with the Agent tab's
ending line, and the composer with the item as a chip.

The conversation model and its host are kept: `core::agent_follow_up` and
`application::follow_up_host` (one conversation per item, fresh tool-free
rounds, proposals that apply only on Use).

## Acceptance

- AC1. The follow-up window (`qt::follow_up_window`), its tests and its
  documentation are gone; no window opens for Ask follow-up or Ask about this.
- AC2. The side chat column shares the tour side chat's parts (the author's
  bubble, the proposal frame, Return-to-send, the composer field, the
  passage chip, the seam), moved into one shared unit both use, not copied.
- AC3. Ask follow-up on a report finding opens the side chat beside the
  report, about that finding (its place and suggested comment as the chip);
  asking shows the question and the agent's answer as turns; Update
  suggested comment shows the proposal beside the comment it replaces with
  Use and Discard; Use updates the finding's suggested comment and the
  report says so. An Agent tab card's Ask follow-up opens the same chat.
- AC4. Ask about this on the draft composer opens the side chat about the
  draft and offers Update draft (one undoable edit, refused over a changed
  draft); on a thread with an empty reply field it offers Draft reply, which
  puts the reply in the thread's reply composer. Nothing is posted.
- AC5. Closing the column keeps every item's chat; opening the same item
  again shows its turns.
- AC6. Qt tests for the side chat (open, ask about an item, revise a
  suggested comment and a draft, draft a reply, close and reopen), the public
  path course moved onto the side chat, `architecture_boundaries`,
  `coding-format-check` and the documentation gate pass.

## Decisions

- The frontend-design skill was loaded for the column.
