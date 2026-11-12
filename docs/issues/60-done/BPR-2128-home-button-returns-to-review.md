---
schema: "implement-rfc.card.v1"
id: "BPR-2128"
title: "Home button becomes Return while Home is over an open review"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2128--BPR-2128--e494160afb4c.json","source_commit":"4096b922e"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4096b922e","evidence":{"kind":"command","detail":"test_home_button_returns_to_the_open_review (--home-return pass): no PR shows Home; Home over a PR shows 'Return to #N title' with the return glyph, unchecked; click or Cmd-Shift-H restores the review with focus; a second PR is named after it opens."}},{"criterion":"AC2","outcome":"pass","revision":"4096b922e","evidence":{"kind":"command","detail":"qt_composed_github_scale_100/125/200, qt_workbench_window_scale_100, qt_button_faces, architecture_boundaries, format and tidy pass."}}]
---

# Home button becomes Return while Home is over an open review

Owner request: on Home with a pull request open, the Home button should return
to it. The action shows a return glyph and "Return to #N title" (full title in
the tooltip and accessible description), is not checkable in that state, and
brings the review back as it was left. Cmd-Shift-H keeps its toggle meaning.

## Acceptance

- AC1. On Home over an open review the button names that review and returns to
  it; otherwise it is Home.
- AC2. Existing window and button-face checks pass.

Open: a cached overview without an accepted review behind Home still shows Home.
