---
schema: "implement-rfc.card.v1"
id: "BPR-2054"
title: "Compose review toggles the review sheet open and closed"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2054--BPR-2054--e98bce0b341f.json","source_commit":"3c24d7d92"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3c24d7d92","evidence":{"kind":"command","detail":"test_compose_review_toggles_the_review_sheet: click opens and checks, second click closes and unchecks, typed text survives reopen, sheet Close unchecks, Option-R toggles, expanded page closes."}},{"criterion":"AC2","outcome":"pass","revision":"3c24d7d92","evidence":{"kind":"command","detail":"application_review_navigation_entry updated to toggle; qt_compose_review_entry_scale_100/200, qt_workbench_window_scale_*, qt_option_keys_typing, format and tidy pass."}}]
---

# Compose review toggles the review sheet open and closed

Owner request: the toolbar "Approve or comment" action should open and close
the compose review sheet.

## Acceptance

- AC1. The action is checkable; clicking it (or Option-R) while the sheet or
  expanded review page is open closes it, keeping the typed review.
- AC2. Its checked state follows every way the sheet opens and closes.
