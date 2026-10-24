---
schema: "implement-rfc.card.v1"
id: "BPR-1757"
title: "Drawer surfaces keep their words at their narrowest and widest widths"
status: "Done"
rfcs: ["RFC-BPR-0085"]
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1757--BPR-1757--4dd2fb87d80d.json","origin":"BPR-1595's closeout attempt, 2026-10-23: RFC-BPR-0085 AC08 failed 68 states at each scale","related_cards":["BPR-1595","BPR-1587","BPR-1593","BPR-1603","BPR-1684"],"frontend_design":"frontend-design:frontend-design loaded before the layout repairs","requested_skill":"frontend-design","milestone":"drawer-closeout"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"270077e655d630ad8afa687046b6bb48bc4f0067","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^qt_drawer_surfaces_fit_scale_(100|125|200)$' (and BPR_DRAWER_FIT_RENDER_ROOT renders at 100 and 200 percent)","detail":"Every state passes at 100, 125 and 200 percent in Light, Dark and both high-contrast themes: Status at 260 and 560; Comments with the edge chip and a review summary open in place, Notes, the Tour route and the Agent tab at 280 and 760, restored and dragged; the Compose sheet, its page and the tour page at the 900-pixel window floor and in a 1920-pixel window. At 6c998babb the same course failed 68 states at each scale. Renders reviewed offscreen: four named tabs at 760 and the open tab named at 280 with no scroll arrows; Changes requested whole at 280; Enhance text whole beside More at 280; the tour page's columns inside the window at its floor and 290 and 360 pixels wide in the wide window."}},{"criterion":"AC2","outcome":"pass","revision":"270077e655d630ad8afa687046b6bb48bc4f0067","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^(qt_workbench_window_scale_.*|qt_drawer_parts_scale_.*|qt_conversation_cards_view|qt_github_conversation_panel|qt_private_notes_panel|qt_review_submission_editor_scale_.*|qt_pull_request_overview_page|qt_composed_inline_drawer_scale_.*|qt_scoped_prose_editor|application_review_tour_composition_.*|application_review_tour_publication_.*|application_review_tour_writing.*)$'; ctest -I 5,6 (architecture_boundaries, architecture_boundary_contract); _coding-standard/scripts/format_check.py and tidy.py --warnings-as-errors on the changed files","detail":"34 of 34 focused rows (the fit course included) and both architecture rows pass on macOS 26 arm64, Qt 6.12, Debug, offscreen; clang-format 22.1.8 and clang-tidy pass on every changed file."}}]
---

# BPR-1757: Drawer surfaces keep their words at their narrowest and widest widths

[[BPR-1595]]'s closeout attempt found RFC-BPR-0085 AC08 unmet: the course
`qt_drawer_surfaces_fit_scale_{100,125,200}` failed 68 states at each scale,
in four defects. This card repairs them as layout, not by shrinking type,
and keeps the owner-accepted looks of [[BPR-1587]] to [[BPR-1594]] and
[[BPR-1603]] unchanged at normal widths.

## Scope

- **The right tray's tab row** (`workbench_window::fit_tray_tabs`). A hidden
  `QTabBar` only marks its tabs dirty when a face changes, and the header's
  own `sizeHint()` query then lays them out. So the bar's `showEvent` had
  nothing left to announce, and the header's grid kept its first width,
  192 pixels. The window opens on Overview with the tray hidden, so a
  restored width left Tour and Agent behind scroll arrows, and dragging to
  760 changed no face and so never corrected it. The row now calls
  `updateGeometry()` itself when a face changes. The window course
  `test_the_right_tray_tabs_keep_their_names_while_they_fit` missed this:
  it shows the window with the tray already visible, where `QTabBar`'s
  `refresh()` announces each change, and reaches every width by dragging.
- **The open review summary** ([[BPR-1603]]'s surface). Its header's
  "Pull request" yields its room first: it elides, says itself whole in its
  tooltip, and shows nothing rather than a lone ellipsis, so "Changes
  requested" stays whole at 280 pixels. The section's "On the pull request"
  heading still names the scope. A post's painted author name, which the
  course cannot measure and which was cut mid-word at 280, now elides
  before the post's tools and shows the whole name in its tooltip.
- **The tour stop's actions.** They sit on a row that keeps to the stop's
  width. Enhance text and More always show; Use selected lines, Related
  source and Remove link show, in that order, while they fit whole, and
  otherwise wait behind More, whose menu already offers each. The route
  already kept Related source behind More ([[BPR-1593]]). The `tourStopCard`
  that measured 355 pixels in the page's 266-pixel outline was the route's
  card: `removeItemWidget` deletes it later, and until then it stood in the
  outline. It is hidden as it is removed.
- **The tour page.** The outline and On GitHub keep 290 and 360 pixels while
  the stop between them has 340; below that they give way in proportion, to
  230 and 290, so at the 900-pixel window floor nothing runs past the
  window's edge (On GitHub ran to 955). The brief's two-line rows now lay out
  their first line and elide the rest on the second; the old estimate showed
  part of a third line in a narrower outline.

## Acceptance criteria

- AC1: `qt_drawer_surfaces_fit_scale_{100,125,200}` pass in every theme and
  state, with offscreen renders reviewed.
- AC2: The workbench window, drawer, conversation, notes, submission editor,
  overview and tour courses stay green; format and tidy pass on the changed
  files.

## Verification

At `270077e65` (macOS 26 arm64, Qt 6.12, Debug, offscreen): AC1 and AC2
pass as recorded in the front matter.

## Not covered

- Windows and Linux are unmeasured; the course ran on macOS.
- Not clipping, so not repaired here: in high-contrast dark, the tour page's
  "Update PR description after publishing" checkbox draws no visible box.
  It does so in the wide window too, where these changes leave its column
  at 360 pixels, so it is the theme's and not these repairs'.
