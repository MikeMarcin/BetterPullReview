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

## Outcome

The Compose review / Finish your review sheet closes (its words kept, as Close keeps them) when I click into the diff, a drawer or the review bar, or choose another mode tab. Opening it again shows it as it was. A test drives each of those gestures; owner look on a frozen build.

