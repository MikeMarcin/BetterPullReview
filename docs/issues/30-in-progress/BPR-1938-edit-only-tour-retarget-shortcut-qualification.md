---
schema: "implement-rfc.card.v1"
id: "BPR-1938"
title: "Qualify the Tour retarget shortcut in its accepted edit-only mode"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"verification","found_by":"v0.4.2 build37 Mac focused scale courses","related_cards":["BPR-1241","BPR-1936"],"milestone_plan":{"id":"bpr-1938-retarget-fixture","path":"docs/rfcs/plans/bpr-1938-retarget-fixture/milestone-plan.json"}}
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

One keyboard qualification milestone owns these three criteria. Accepted
BPR-1936 makes both the card link and route/More QAction visible only while
writing. Qt correctly withholds the hidden action's shortcut while reading.
The old BPR-1241 fixture selected a stop but never entered edit mode, so its
old positive expectation no longer described the accepted command. Its key
and focus boundaries remain required; no assertion is dropped and no product
availability or shortcut binding changes are needed.

The release coordinator owns frozen build37, active producer jobs and future
allocation/publication. This slice uses its own branch and offscreen tests;
no owner desktop or release artifacts are touched.

## Additional keyboard catalogue observation

After all three full window scale rows passed, the focused keyboard catalogue
course correctly rejected BPR-1936's unnamed Escape shortcut under
`tourStopRemoveConfirm`. Its existing binding was absent from the catalogue.
The correction names that same shortcut `tourStopRemoveKeepKey` and derives its
unchanged Escape key from a new catalogue entry, "Keep the stop", scoped to
the removal question. The WidgetWithChildren context and Keep-button action
stay unchanged. The card course now drives Escape through the window system
and verifies that it keeps the stop and edit, closing only the question.
This is existing-binding inventory work under the same keyboard acceptance,
not a new key choice or command behavior. Frontend-design guidance was retained
for the catalogue wording; the accepted card layout and controls are unchanged.


## Verification and integration

The frozen release source is `e193fba8af805f4b36bc03a92918ccf64d452375`
(build37). Its original three scale failures are retained in external
`candidate37/focused-tests.xml` and its CLI-enabled retry. No release source,
package, tag or native producer was changed by this implementer.

On the isolated source slice, the required catalogue check first exposed the
uncatalogued Escape binding (`implementer1938/focused.xml`). The first new
Escape regression retained a failed active-focus precondition
(`catalogue-focused.xml`); the fixture now explicitly activates its offscreen
window after the menu sequence before focusing Keep. The real window-system
key path then passes. No product focus behavior was changed.

Final focused checks pass all six courses: `qt_dif_workshop_placement`,
`qt_keyboard_shortcuts_scale_100`, `qt_keyboard_shortcuts_scale_125`,
`qt_keyboard_shortcuts_scale_200`, `qt_review_tour_stop_view` and
`qt_tour_stop_card_cleanup`. The last course proves Escape keeps the stop and
its edit and closes only the removal question; its existing explicit Remove
and Shift-click checks remain. The composed retarget course adds read and
collapsed negative checks, edits by the actual pencil, and verifies exactly
one retarget on each press, including after reopening. Option-R, page identity
and non-outline guards remain.

Evidence lives externally under
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/candidate37/implementer1938/`.
It includes raw failures, final focused and full scale XML/logs, formatting and
documentation results, plan amendment/binding receipts and final handback.
The bounded plan refinement records only the existing Escape binding inventory;
all original obligations and exits remain. Native release qualification and
publication are separately owned by the coordinator; these offscreen source
checks do not grant release acceptance.
