---
schema: "implement-rfc.card.v1"
id: "BPR-1488"
title: "A held Review navigation press reads its tab's current view"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1486--BPR-1491--162208d134c7.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) fails on main at cd4860e30, owner release request 2026-10-13","related_rfcs":["RFC-BPR-0076"],"related_cards":["BPR-1417"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"993e07c91","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_review_round_trip_composition_contract|application_uiforetw_selected_diff_contract|application_workbench_composition_contract|application_start_review_contract|application_appearance_theme_customization_contract|live_review_submission_provider|application_review_navigation_entry|application_change_arrows_across_files)$' --output-on-failure","detail":"A held press waits for the active tab current view; each tab publication re-evaluates it; system-architecture.md says so."}},{"criterion":"AC2","outcome":"pass","revision":"993e07c91","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_review_round_trip_composition_contract|application_uiforetw_selected_diff_contract|application_workbench_composition_contract|application_start_review_contract|application_appearance_theme_customization_contract|live_review_submission_provider|application_review_navigation_entry|application_change_arrows_across_files)$' --output-on-failure","detail":"Workbench wrap step waits with a named stall; workbench composition, change arrows across files and review navigation entry pass (5.35 s, 15.39 s, 38.90 s)."}},{"criterion":"AC3","outcome":"pass","revision":"993e07c91","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_review_round_trip_composition_contract|application_uiforetw_selected_diff_contract|application_workbench_composition_contract|application_start_review_contract|application_appearance_theme_customization_contract|live_review_submission_provider|application_review_navigation_entry|application_change_arrows_across_files)$' --output-on-failure","detail":"Round-trip composition contract resealed with appended note; passes (3.75 s)."}}]
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

## Evidence

- Cause: 40639f2f6 (RFC-BPR-0076) held a press until its remembered reading position was presented; a selection made just before the press reaches the tab a lane turn later, so the press waited forever. Fix d4aeadf64: wait for the active tab's current view, re-evaluated at each tab publication; architecture note 389400918. The workbench course's wrap step waits with `WORKBENCH_WRAP_DISPATCH_STALLED`; the owner's session observed it fail on that stall with the product change reverted.
- Baseline gate on cd4860e30: `application_workbench_composition_contract` and `application_change_arrows_across_files` ("the arrow stays enabled and the status bar says why nothing moved") both failed. At 993e07c91 both pass (5.35 s, 15.39 s), and `application_review_navigation_entry` passes (38.90 s).
- AC3: `application_review_round_trip_composition_contract` resealed (full application and production-manifest digests) with an appended [[BPR-1488]] note; it passes (3.75 s).
- Focused run: `ctest --preset dev -R '^(application_review_round_trip_composition_contract|application_uiforetw_selected_diff_contract|application_workbench_composition_contract|application_start_review_contract|application_appearance_theme_customization_contract|live_review_submission_provider|application_review_navigation_entry|application_change_arrows_across_files)$' --output-on-failure`: all eight pass.
