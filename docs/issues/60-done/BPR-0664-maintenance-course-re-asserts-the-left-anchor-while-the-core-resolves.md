---
schema: "implement-rfc.card.v1"
id: "BPR-0664"
title: "Maintenance course re-asserts the left anchor while the core resolves"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
acceptance: [{"criterion":"application_epoch_maintenance_public_path passes at the fixed head, alone and beside the other epoch-phase contracts","outcome":"pass","revision":"c61f03ce","evidence":{"kind":"observation","detail":"Fails at 60ad21c3 alone (budget expired at stage 15 after 120 s, epoch status Resolving the exact comparison…); at the fix passes alone (50 s) and in one serial batch with application_epoch_persistence_github-com and application_epoch_persistence_ghes-3_18. application_review_round_trip_composition_contract repinned in two passes and green, application_composition_contract green; strict tidy on main.cpp, clang-format --Werror and verify_documentation.py pass."}}]
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
the race [BPR-0655](BPR-0655-defect-composition-seed-stalls-resolving-the-comparison.md)
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

## Notes (2026-09-07)

- **Fix.** `epoch_maintenance_select_right` now waits while
  `status().EpochStatus` is `resolving` before reading the left selector,
  and only a settled core showing another row is re-asserted. The stage's
  earlier re-assert stays for the case it was written for (a projection
  repopulating the model and clearing the selection between ticks).
- **Evidence.** `application_epoch_maintenance_public_path` fails at
  60ad21c3 alone (budget expired at stage 15, 120 s) and passes at the fix
  alone (50 s) and beside `application_epoch_persistence_{github-com,ghes-3_18}`
  in one serial batch. No other course re-asserts a selector on every tick:
  the remaining `setCurrentIndex` sites set once after the pair they wait
  for has passed.
