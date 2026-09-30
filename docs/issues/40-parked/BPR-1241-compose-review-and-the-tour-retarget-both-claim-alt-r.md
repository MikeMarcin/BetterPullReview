---
schema: "implement-rfc.card.v1"
id: "BPR-1241"
title: "Compose review and the tour editor's retarget both claim Alt+R"
status: "Parked"
rfcs: []
depends_on: []
metadata: {"origin":"recorded as pre-existing under BPR-1157 (RFC-BPR-0053-0061 ledger); filed by the main stabilization milestone on 2026-09-29","reservation":"docs/issues/_reservations/BPR-1240--BPR-1243--20fe98015ceb.json","related_cards":["BPR-1157","BPR-1129","BPR-1135"]}
parked_on: "human-review"
acceptance: [{"criterion":"The six scale rows pass on macOS","outcome":"pass","revision":"ddecba284","evidence":{"kind":"command","detail":"qt_document_tabs_scale_{100,125,200} and qt_workbench_focus_order_scale_{100,125,200} pass on macOS arm64 offscreen."}},{"criterion":"application_review_tour_publication_public_path still retargets through the action","outcome":"pass","revision":"ddecba284","evidence":{"kind":"command","detail":"application_review_tour_publication_native and _compat pass after the app and course were rebuilt."}},{"criterion":"Owner judgment of the proposed retarget key","outcome":"pending","revision":"ddecba284","evidence":{"kind":"freeform","detail":"Attended item BPR-1241-retarget-key-v001, session cbd48b91000b00f1e72e1ad62c7c384c, ready and protected; no server started."}},{"criterion":"Documentation gate","outcome":"pass","revision":"ddecba284","evidence":{"kind":"command","detail":"python3 scripts/verify_documentation.py --root . -> DOCUMENTATION_OK"}}]
---

# BPR-1241: Compose review and the tour editor's retarget both claim Alt+R

## Observation

At `main` e9e65eab2 on macOS arm64, `qt_document_tabs_scale_{100,125,200}`
fail at `assembled keys are unique` and
`qt_workbench_focus_order_scale_{100,125,200}` at `no keyboard command is
claimed twice in the assembled workbench`. Two actions in one workbench window
carry `Alt+R`:

- `composeReviewAction`, Compose review (`src/adapters/qt/src/workbench_window.cpp`),
  the owner-accepted Option-R of RFC-BPR-0052 AC-10 ([[BPR-1129]]);
- `tourRetarget`, "Use selected source as replacement" in the tour outline's
  stop menu (`src/adapters/qt/src/review_tour_editor.cpp`), added by 54ba51bf8
  on 2026-09-27.

Qt treats a key two enabled actions in one window match as ambiguous and runs
neither, so with the tour outline focused Option-R neither composes the review
nor retargets the stop. That is why the rows require one claim per key.

## Outcome

Compose review keeps Option-R. The retarget moves to a key nothing else in the
window claims. The key is a UX choice, so the owner accepts or rejects the
proposal.

## Direct acceptance

- The six scale rows pass on macOS.
- `application_review_tour_publication_public_path` still retargets through the
  action.
- Owner judgment: the proposed retarget key (attended item `BPR-1241-retarget-key-v001`).
- `python3 scripts/verify_documentation.py --root .` passes.

## Candidate (macOS arm64, 2026-09-29)

`ddecba284` moves `tourRetarget` to `Alt+Shift+U` (Option-Shift-U), which no
other action in the assembled window claims; Compose review keeps Option-R.

| Check | Result |
|---|---|
| `qt_document_tabs_scale_{100,125,200}`, `qt_workbench_focus_order_scale_{100,125,200}` | Pass (they failed at the duplicate key before) |
| `application_review_tour_publication_{native,compat}`, which trigger `tourRetarget` | Pass |
| Tour editor Qt rows | Pass |

Owner judgment is parked as attended item `BPR-1241-retarget-key-v001` in
session `cbd48b91000b00f1e72e1ad62c7c384c` (prepared and protected, no server
started): Accept keeps Option-Shift-U; Reject names another key in the note.

## Linux — bpr-main-linux, 2026-09-30

Ubuntu 26.04 on WSL2 (kernel 6.18.33.2), GCC 15.2, clang-format 22.1.8 at `/usr/bin/clang-format`, Qt 6.11.2 offscreen with `QT_STYLE_OVERRIDE=Windows` (default font DejaVu Sans), `build/dev` (Debug, `-Werror`) freshly configured at `69493e235`, which contains the Windows station's `0dd1eb53f`.

Evidence only; the card stays parked for the owner. `qt_workbench_focus_order_scale_{100,125,200}` pass, so the Alt+R conflict is gone here too. `qt_document_tabs_scale_{100,125,200}` fail on Linux at a different check: `activation reveals the entire active tab including after restoration`. A temporary trace, reverted and never committed, measured it with one tab at the window's floor: the strip's viewport is 179 px, but `document_tab_strip::minimum_tab_width` is 220, so the active tab (rect −41..178) cannot be fully revealed. The Linux font makes the chrome around the strip wider than on the Mac. Whether the floor, the minimum tab width or the check should give is a layout decision, so nothing was changed.

The logs are station records in `build/logs/bpr-station-stabilization/` and are not committed.
