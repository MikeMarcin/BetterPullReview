---
schema: "implement-rfc.card.v1"
id: "BPR-2216"
title: "Opening a pull request from Home is responsive: its page at once, Return and the Active review while it loads"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac","win","linux"]
metadata: {"kind":"ux","found_by":"Owner chat, 2026-11-16","reservation":"docs/issues/_reservations/BPR-2216--BPR-2216--0ff5959e3c1a.json","related_cards":["BPR-1200","BPR-1357","BPR-1365","BPR-1599","BPR-1842","BPR-1843","BPR-2180"],"related_rfcs":["RFC-BPR-0032","RFC-BPR-0026"]}
---

# BPR-2216: Opening a pull request from Home is responsive: its page at once, Return and the Active review while it loads

## Owner notes

Owner, chat, 2026-11-16, verbatim: "Right now a lot of things are slow to
react when I open a pull request from home. The pull request window doesn't
open very quickly. If I click home from a pull request that's not finished
loading it doesn't change it to the return button and it doesn't have the
active pull request near the top of home. It would be nice to make this more
responsive."

## Acceptance

Timing is measured in an optimized Release build only. Debug timings are
permanently nongating (AGENTS.md, "Debug timings").

- **BPR-2216:page-at-once.** Opening a pull request from a row Home lists
  (the queue's double-click or Return, or a Recently viewed row) shows that
  pull request's page in the gesture's own event-loop turn, before GitHub has
  answered anything: the window leaves Home for the pull request's Overview,
  its title row names the pull request, and the page says it is loading,
  filling in progressively as the read's parts arrive ([[BPR-1842]],
  [[BPR-2180]]), never a frozen or blank wait. Measurably, in Release on the
  fixture GitHub server with the read's first answers held for seconds: the
  first frame of that page is painted within 100 ms of the double-click, and
  no single event-loop turn between the click and that frame takes longer
  than 100 ms.
- **BPR-2216:return-while-loading.** Home pressed while that pull request is
  still loading shows Home with the toolbar's Home button as the way back to
  it: "Return to #N …", the return glyph, unchecked, and pressing it returns
  to the loading page, exactly as it does for a loaded review
  ([[BPR-1843]]; the owner's request of 2026-11-11).
- **BPR-2216:active-while-loading.** Home pressed while that pull request is
  still loading lists it near the top as the active pull request: the Active
  review card names it (owner/name#N and its title) with Back to the review,
  and its queue row carries the reviewing mark ([[BPR-1365]]), exactly as
  for a loaded review.
- **BPR-2216:decided-in-core.** Whether a pull request is the window's active,
  returnable one is decided from the open request the core accepted (its
  request identity), not from the read's completion; the host does the read
  and its effects off the UI thread; Qt only presents the decision.
- **BPR-2216:courses.** A composed public-path course on the fixture GitHub
  server with a slow first read checks each criterion above through the real
  window offscreen, and fails on the code before the change.
- **BPR-2216:owner-look.** Owner Accept of a frozen fixture-agent build that
  opens a slow-loading pull request from Home, one question per criterion.
