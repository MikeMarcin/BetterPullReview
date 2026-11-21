---
schema: "implement-rfc.card.v1"
id: "BPR-2335"
title: "An Agent tab turn makes the conversation pan sideways at the drawer's narrowest width"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","origin":"BPR-2331's summary test, 2026-11-20","reservation":"docs/issues/_reservations/BPR-2335--BPR-2335--811f07397c0a.json","related_cards":["BPR-2331"],"acceptance":["AC1","AC2"]}
---

# BPR-2335: An Agent tab turn makes the conversation pan sideways at the drawer's narrowest width

## Found

While [[BPR-2331]]'s summary test measured the Agent tab at the
conversations drawer's narrowest width (280 points, offscreen, macOS), the
conversation scroll area (`agentConversationScroll`) had a horizontal range
of 5 points: its content needed 267 points of a 262 point viewport. At a
1.6 times application font it needed 69 points more. The Qt adapter's rule
is that no drawer ever pans sideways (`src/adapters/qt/AGENTS.md`).

It is not the report summary or the finding cards: with the summary hidden,
and with each shown card hidden in turn, the content still needed 267
points. Hiding single widgets did not lower it, so more than one turn part
(each turn's reply needs about 243 points) holds it there. The likely
holders are a reply's controls row (Copy, Keep local, Draft comment, the
time) and the question bubble; not yet confirmed.

`qt_drawer_surfaces_fit_scale_100/125/200` checks every drawer tab at its
narrowest, but not an Agent tab holding answered turns.

## Acceptance

- AC1. An Agent tab with an answered turn, a report turn and a turn with a
  finding card has no horizontal range at the conversations drawer's
  narrowest width, at 1.0, 1.25 and 2.0 scale, and with a larger
  application font (rows wrap, as `detail::wrapping_row` does).
- AC2. `qt_drawer_surfaces_fit_scale_*` covers the Agent tab with answered
  turns.
