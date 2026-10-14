---
schema: "implement-rfc.card.v1"
id: "BPR-1488"
title: "A held Review navigation press reads its tab's current view"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1486--BPR-1491--162208d134c7.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) fails on main at cd4860e30, owner release request 2026-10-13","related_rfcs":["RFC-BPR-0076"],"related_cards":["BPR-1417"]}
---

# A held Review navigation press reads its tab's current view

`application_workbench_composition_contract` fails on main. 40639f2f6
(RFC-BPR-0076) holds a Next/Previous change press until Review shows the
active tab's exact view, but the held press remembered the reading position
from the moment of the press. A selection made just before the press reaches
the tab a lane turn later, so the remembered position is never on screen
again and the press is never applied: a user can lose a key press in that
window. Diagnosed in the owner's session "Diagnose four failing GUI
public-path contracts" (d4aeadf64, 389400918).

## Acceptance

- AC1: A held press waits for the active tab's current view, and each tab
  publication re-evaluates it. RFC-BPR-0076 binds the press to its pull
  request, tab and comparison, not to a remembered reading position; the
  system architecture says so.
- AC2: The workbench course's wrap step waits for the dispatch with a named
  stall, fails on that stall with the change reverted, and passes with it;
  `application_review_navigation_entry` still passes.
- AC3: `src/app` changed, so the round-trip composition contract is resealed
  with an appended note.

## Boundaries

No change to which gestures cancel a held press.
