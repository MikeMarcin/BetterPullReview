---
schema: "implement-rfc.card.v1"
id: "BPR-2175"
title: "Compose review hides when I navigate elsewhere"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation": "docs/issues/_reservations/BPR-2172--BPR-2175--e382f3fed4fa.json", "origin": "owner aside, look v007 of RFC-BPR-0127 (attended session efe2da80b42f60b91e24da153dc8ea5b, handed back 2026-11-14T01:50:15Z)"}
---

# BPR-2175: Compose review hides when I navigate elsewhere

The owner, in the same note on `finish-review-local-line`:

> I feel like clicking off compose review into the diff or drawers or review bar or using the mode tabs should hide it. It feels more in the way than helpful staying open when i'm trying to navigate the app.

Then, on this card:

> hmm maybe compose review should use the light box we mocked up but decided not to use for big agent responses instead

And:

> Yeah just make sure we can still do basic window controls while lightbox stays open like drag window or resize it

## Proposed direction (needs a mockup look before building)

Compose review / Finish your review opens as the lightbox from the Ask mockups, the variant mocked for big agent responses and not chosen there ([docs/design/ask-v1](../../design/ask-v1/README.md), `png/lbx-agent-light.png` and `png/lbx-tour-light.png`). It is dismissed by a click outside it, Esc, or a mode tab, keeping its words as Close does; opening it again shows it as it was.

## Outcome

1. A mockup of Compose review as that lightbox (light and dark, the narrow and wide windows), shown to the owner as an attended look, one Accept/Reject. No building before it is accepted.
2. Then: a click inside the window's content outside the lightbox (the diff, a drawer, the review bar), Esc, or choosing another mode tab closes it with its words kept; a test drives each gesture; owner look on a frozen build.
3. While the lightbox is open the window keeps its own controls: it can be dragged by its title bar, resized, minimised, zoomed and made full screen. The lightbox stays open and re-fits the new size. Window-chrome interaction never dismisses it; only the gestures in 2 do. A test resizes, minimises and restores the window with the lightbox open and checks it is still open and fitted.
