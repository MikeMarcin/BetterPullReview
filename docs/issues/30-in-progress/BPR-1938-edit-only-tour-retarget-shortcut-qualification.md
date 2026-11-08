---
schema: "implement-rfc.card.v1"
id: "BPR-1938"
title: "Qualify the Tour retarget shortcut in its accepted edit-only mode"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"verification","found_by":"v0.4.2 build37 Mac focused scale courses","related_cards":["BPR-1241","BPR-1936"]}
---

# BPR-1938: Edit-only Tour retarget shortcut qualification

## Acceptance

- AC1. Reconcile the composed keyboard fixture with accepted BPR-1936:
  Use selected lines is unavailable while reading/collapsed, and Option-Shift-U
  fires exactly once from the focused outline while editing a selected stop.
- AC2. Preserve BPR-1241's key, no-unrelated-page change, Option-R composition
  and non-outline negative assertions; run all three window scales and focused
  Tour/card/keyboard courses on the corrected source.
- AC3. Format, documentation and main integration pass. Retain build37's actual
  three failures and avoid source changes to its active frozen build.

## Diagnosis and bounded milestone

One fixture/documentation milestone owns these three criteria. Accepted
BPR-1936 makes both the card link and route/More QAction visible only while
writing. Qt correctly withholds the hidden action's shortcut while reading.
The old BPR-1241 fixture selected a stop but never entered edit mode, so its
old positive expectation no longer described the accepted command. Its key
and focus boundaries remain required; no assertion is dropped and no product
availability or shortcut binding changes are needed.

The release coordinator owns frozen build37, active producer jobs and future
allocation/publication. This slice uses its own branch and offscreen tests;
no owner desktop or release artifacts are touched.
