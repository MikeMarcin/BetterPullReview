---
schema: "implement-rfc.card.v1"
id: "BPR-2173"
title: "Opening the bottom comment in the drawer scrolls the drawer wrongly"
status: "Done"
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


## Progress (2026-11-14, branch `claude/parallel-backlog-2156-2172-2173`)

Reproduced first in `qt_conversation_cards_view`
(`test_opening_the_card_below_an_open_thread_shows_it_whole`): a tall thread
open at line 6, the drawer scrolled to my Local comment under it, opening that
comment left its top at -118 px (scroll 214). `conversation_cards_view::reveal_opened`
now scrolls the notes, no further than their end needs, so the opened card's
top is clear of the edge chips and its foot is in view; a card whose line is
off the code's view is still reached by the code moving. Pending: the owner's
look on the same fixture.

## Owner look (session `9afc3306d0cf788e0c2eaaec43d5b882`, frozen `200ba998fa15`, handed back 2026-11-14)

- `BPR-2173-drawer-scroll`: **Accept** (no note), at 2026-11-14T16:37:13Z.

Both outcomes hold: reproduced and fixed under test (`qt_conversation_cards_view`),
and the owner's look on the Local comments look's fixture file. Done.
