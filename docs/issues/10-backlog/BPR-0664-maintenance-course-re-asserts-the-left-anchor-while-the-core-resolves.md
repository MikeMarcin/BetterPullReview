---
schema: "implement-rfc.card.v1"
id: "BPR-0664"
title: "Maintenance course re-asserts the left anchor while the core resolves"
status: "Backlog"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
---

# Maintenance course re-asserts the left anchor while the core resolves

`application_epoch_maintenance_public_path` fails at 60ad21c3, alone and
with the BPR-0654 slice stashed: the run spends its 120-second budget in
`epoch_maintenance_select_right` (stage 15) with the epoch status
"Resolving the exact comparison… The retained comparison remains visible.
An obsolete review-history result was ignored." The stage sets the left
selector and, on every 10 ms tick where the selector does not show that
row, sets it again. The selector control blanks the combo the moment the
core starts resolving the anchor it dispatched, so each tick dispatches a
new `select_comparison_anchor_intent`, each bump cancels the previous
resolution, and every completion arrives one generation stale. This is
the race [BPR-0655](../60-done/BPR-0655-defect-composition-seed-stalls-resolving-the-comparison.md)
found in the defect probe; the stage's own comment records the re-assert
as the cure for a one-in-seven flake, which it was while a resolution
landed inside one tick.

## Scope

- Wait while the epoch host reports `resolving` before reading the left
  selector as "not applied", and only re-assert against a settled core
  that shows another row, as BPR-0655 did for the defect probe.
- Owned paths: `src/app/main.cpp` (the maintenance course) and the
  contract under `tests/end_to_end`.

## Acceptance

- `application_epoch_maintenance_public_path` passes at the fixed head,
  alone and beside the other epoch-phase contracts.
