---
schema: "implement-rfc.card.v1"
id: "BPR-2260"
title: "A draft clicked away from keeps its words"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2260--BPR-2260--1c6312374c89.json","origin":"owner, 2026-11-18"}
---

# A draft clicked away from keeps its words

## Origin

The owner, 2026-11-18, with photos of the drawer: "I have a draft comment
sometimes not rendering when I click away from it." Drawn correctly, my
draft's note names me, its line and Draft and shows two lines of its words;
after another thread is clicked open it is an empty dashed frame, shorter,
with no avatar, author or words. No look is needed.

## Scope and acceptance

- **AC-a.** My draft written in the drawer, then another thread opened by a
  click (or the thread closed), my draft's note carries its post: author,
  line, Draft and its words, measured as a fresh view measures it.
- **AC-b.** Rendered offscreen, the note paints ink inside its dashed frame.
- **AC-c.** Opening the draft again makes it the composer again.
- **AC-d.** A Qt test fails without the fix and passes with it.
