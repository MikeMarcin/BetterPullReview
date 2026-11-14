---
schema: "implement-rfc.card.v1"
id: "BPR-2173"
title: "Opening the bottom comment in the drawer scrolls the drawer wrongly"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation": "docs/issues/_reservations/BPR-2172--BPR-2175--e382f3fed4fa.json", "origin": "owner aside, look v007 of RFC-BPR-0127 (attended session efe2da80b42f60b91e24da153dc8ea5b, handed back 2026-11-14T01:50:15Z)"}
---

# BPR-2173: Opening the bottom comment in the drawer scrolls the drawer wrongly

The owner, aside on `draft-refused-struck-pencil` (Accept), with two screenshots ([image 1](../../verification/RFC-BPR-0127-v007-asides/drawer-scroll-image-1.png), [image 2](../../verification/RFC-BPR-0127-v007-asides/drawer-scroll-image-2.png)):

> Aside: reproducible bug on comment drawer scrolling, going from image 2 and clicking the bottom comment gives image 1.

Seen on the frozen `--attended-local-comments-look` (conventional-comments-v1, src/engine/renderer.cpp): with line 6's thread open (image 2), clicking the Local comment card below it leaves the drawer scrolled so the opened card is cut at the top and the list below is empty space (image 1).

## Outcome

Reproduced in a Qt or composed test first. Opening a card below an open thread brings the opened card into view whole (its top below the drawer's tools row), with no stretch of empty drawer under it. Owner look on the same fixture.

