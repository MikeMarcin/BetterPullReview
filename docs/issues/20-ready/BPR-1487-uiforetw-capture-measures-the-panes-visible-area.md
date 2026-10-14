---
schema: "implement-rfc.card.v1"
id: "BPR-1487"
title: "The UIforETW selected-diff capture measures each pane's visible area"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1486--BPR-1491--162208d134c7.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) fails on main at cd4860e30, owner release request 2026-10-13","related_cards":["BPR-1413"]}
---

# The UIforETW selected-diff capture measures each pane's visible area

`application_uiforetw_selected_diff_contract` fails on main. Since
[[BPR-1413]] (60aa86361) each code pane's viewport is moved up by the
within-line offset, made a line taller and masked to the pane's content area.
The capture still measured the whole viewport widget, so the panes appeared to
reach past the comparison. Nothing visible moved. Diagnosed in the owner's
session "Diagnose four failing GUI public-path contracts" (2d990e08a).

## Acceptance

- AC1: The capture measures a pane by its viewport mask when one is set, as
  BPR-1413's own Qt tests do.
- AC2: `application_uiforetw_selected_diff_contract` passes on macOS.

## Boundaries

Probe measurement only; no product rendering change.
