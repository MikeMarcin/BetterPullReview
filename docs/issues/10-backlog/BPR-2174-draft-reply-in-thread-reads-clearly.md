---
schema: "implement-rfc.card.v1"
id: "BPR-2174"
title: "My draft reply in a thread reads clearly and keeps normal undo"
status: "Backlog"
rfcs: ["RFC-BPR-0127"]
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation": "docs/issues/_reservations/BPR-2172--BPR-2175--e382f3fed4fa.json", "origin": "owner aside, look v007 of RFC-BPR-0127 (attended session efe2da80b42f60b91e24da153dc8ea5b, handed back 2026-11-14T01:50:15Z)"}
---

# BPR-2174: My draft reply in a thread reads clearly and keeps normal undo

The owner, aside on `finish-review-local-line` (Accept), with a screenshot of line 6's thread ([image](../../verification/RFC-BPR-0127-v007-asides/draft-reply.png)):

> Aside: i'm really confused at what i'm looking at here, why is this draft reply minimized like that? It also desn't seem to respect normal undo/redo rules. Draft reply seems very funky.

The thread's Reply field shows my draft reply folded ("Draft reply" and its first words, elided) with the state control beside it (RFC-BPR-0127, look v003 rework).

## Outcome

My draft reply in a thread reads as my comment: its words shown as a post would show them, in the thread, with its state; it is not a folded field. Undo and redo in its editor follow the platform's rules (typing, then Undo, restores the text; Redo restores it again; nothing a save or reload does is in the undo stack). Owner look on a frozen build with a draft reply in a thread.

