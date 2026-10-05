---
schema: "implement-rfc.card.v1"
id: "BPR-1279"
title: "Offer an explicit rebind for a moved or replaced cycle folder in the terminal and the window"
status: "Ready"
rfcs: ["RFC-BPR-0054"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-1183 recovery corpus","reservation":"docs/issues/_reservations/BPR-1279--BPR-1279--b3893733ba1f.json","related_cards":["BPR-1183","BPR-1148"]}
---

# BPR-1279: Offer an explicit rebind for a moved or replaced cycle folder in the terminal and the window

## Observation

[[BPR-1183]]'s recovery corpus showed that a project folder which is moved or
replaced can rejoin its cycle only by being moved back. The core supports an
explicit rebind ([[BPR-1148]]), but neither `bpr cycle` nor the window offers
it, so the reviewer has no way forward from the refusal.

## Outcome

Offer an explicit, confirmed rebind of a cycle to its folder's new location (or
to a replacement checkout) in the terminal and the window, through the core's
existing rebind rules, with the refusal words pointing at it. Never rebind
silently. Cover it in the recovery corpus with negative controls.
