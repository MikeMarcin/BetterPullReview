---
schema: "implement-rfc.card.v1"
id: "BPR-2056"
title: "Open card line link draws on the card in the native macOS style"
status: "Done"
rfcs: []
depends_on: ["BPR-2053"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2056--BPR-2056--ecf7de435a2e.json","source_commit":"3b0c1c49d"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3b0c1c49d","evidence":{"kind":"command","detail":"Cocoa macos style, Dark profile on light system: line link 3.32 to 5.22 at rest, 1.77 to 5.22 pressed; HC Dark 2.74 to 7.01. Fusion Dark hover 3.87 to 5.22."}},{"criterion":"AC2","outcome":"pass","revision":"3b0c1c49d","evidence":{"kind":"command","detail":"test_line_numbers_read_on_the_open_card measures against the painted face at rest, hover, pressed and focus; qt_conversation_cards_view, qt_diff_workbench_scale_*, architecture_boundaries and format pass."}}]
---

# Open card line link draws on the card in the native macOS style

Owner photo: in the live app the draft's "Line N" button showed blue words on a
light native bezel. The live macOS style gives auto-raise tool buttons a bezel
following the system appearance, not the app palette; offscreen Fusion never
showed it. The line link now paints itself with no face, underlines on hover
and press, and draws the standard keyboard focus ring.

## Acceptance

- AC1. The line link meets the theme minimum under the native macOS style with
  light and dark system appearance.
- AC2. Its contrast is measured against the painted face in every state.

Not fixed: the card's icon-only tool buttons share the native bezel issue.
