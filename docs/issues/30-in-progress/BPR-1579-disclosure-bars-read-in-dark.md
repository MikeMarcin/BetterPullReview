---
schema: "implement-rfc.card.v1"
id: "BPR-1579"
title: "Disclosure bars read in every theme"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner bug 2026-10-17","owner_note":"In Dark, Overview's \"Check details: 3 failed\" disclosure bar draws white text on a light-grey fill (low contrast, a \"flashbang\"). \"Requirement details\" and \"Pull request and account\" (Compose review) look affected too.","related_cards":["BPR-1513","BPR-1157","BPR-0207"],"reservation":"docs/issues/_reservations/BPR-1579--BPR-1579--a8fa7b7c4cd6.json","accepted_treatment":"BPR-1513 v003 (session 0fec6737, decision d96dea45)"}
---

# BPR-1579: Disclosure bars read in every theme

The owner reported on 2026-10-17 that in Dark the Overview's "Check details:
3 failed" bar draws white text on a light grey fill, the same flash
[[BPR-1513]] removed from the Appearance banners, and that "Requirement
details" and Compose review's "Pull request and account" look affected too:
grey bordered buttons with a chevron.

Every one of these is a plain `QToolButton` with an arrow, so the platform
style draws it. On macOS that is a native bezel whose grey follows the
system appearance rather than the profile, under the Dark profile's
near-white text.

This is a bug fix that applies BPR-1513's accepted v003 treatment (the
theme's raised surface, `QPalette::Light`: Light `#FFFFFF` on `#F6F7F9`, Dark
`#2A3039` on `#1C2026`; the high-contrast themes flat). It adds no new design:
layout and behaviour stay, and the UX redesign may restyle these panels later.

## Acceptance

- AC1: One shared disclosure bar paints every chevron disclosure from the
  theme's surface and text roles, with no style sheet and no second palette.
  The bar is `QPalette::Light`, or the plain surface where a palette's
  `Light` does not read under its text. The title and chevron are the text
  colour.
- AC2: Each affected disclosure, in the four built-in themes, closed, open
  and pressed, renders its title at 4.5:1 or more on the bar, the bar as the
  theme's raised surface and the chevron in the text colour. A negative
  control fails the same reading on the old platform-drawn bar.
- AC3: Behaviour is unchanged: object names, accessible names and
  descriptions, checked state, the arrow type the composed tests read, and
  what each disclosure shows and hides. The focused bar draws the window's
  focus ring.
- AC4: Focused tests, one full `ctest --preset dev` on the merged tree,
  clang-format 22, tidy and the documentation gate pass. Before and after
  renders in Light and Dark are recorded on this card. No new owner review
  is needed because the treatment is the one the owner already accepted.

## Coordination

The frontend-design skill is loaded for a consistency check. The Overview
Open and All switch ([[BPR-1577]]) and the review bar's Updates disclosure
(already painted from the palette) are not part of this card. The owner's
desktop is never driven or captured.

## Execution record

Engineering, 2026-10-17. Frontend-design was loaded first. The check found
nothing new to design: the bar takes BPR-1513's raised surface, the window's
focus ring ([[BPR-1157]]), and the hover tint and triangle of the review bar's
Updates disclosure ([[BPR-1130]]), all from the palette.

### What was affected

All eight chevron disclosures were plain `QToolButton`s with
`setArrowType`, so each was drawn by the platform style:

| Disclosure | Object name | Where |
|---|---|---|
| Check details (the report) | `overviewChecksDisclosure` | Overview, `pull_request_overview_page.cpp` |
| Requirement details | `overviewRequirementDisclosure` | Overview |
| Pull request and account | `wholeReviewDetails` | Compose review, `review_submission_editor.cpp` |
| Skipped sides | `sourceSearchSkippedToggle` | Find in files, `source_search_panel.cpp` |
| About local diagnostics | (none) | Settings, `diagnostic_settings_panel.cpp` |
| Technical details | `githubTechnicalDetailsButton` | `github_pull_request_panel.cpp` |
| Details | `serverCardDetailsButton` | Accounts & hosts, `server_card.cpp` |
| Profile maintenance, Export options, Show records | `profileMaintenanceOptions`, `diagnosticExportOptions`, `diagnosticExportRecords` | `release_maintenance_panel.cpp` |

On macOS the style draws a checked tool button, and any tool button without
auto-raise, as a native bezel. Its grey follows the system appearance, not the
profile, while the title is the profile's `ButtonText`. Under Dark that put
`#E6EDF3` on light grey. The review bar's Updates disclosure was already
painted from the palette and is unchanged.

### The fix

`bpr::qt::disclosure_button` (`src/adapters/qt/include/bpr/qt/disclosure_button.hpp`)
is a `QToolButton` that paints itself. It has no style sheet and no second
palette.

- The bar is `disclosure_fill`: the palette's `Light` (BPR-1513's raised
  surface) when `WindowText` reads on it at 4.5:1, otherwise `Window`. The
  fallback exists for palettes the profile did not derive, such as a
  top-level dialog's platform palette.
- The title and the triangle are `WindowText`. Hover and press tint the
  bar with `Mid`, and focus draws the window's focus ring.
- `arrowType()` follows the checked state, so the hosts lost their
  `setArrowType` calls and the composed tests still read the arrow.
- The size hint rounds the title's fractional advance up, so a bar at its
  own size never elides its title.

The eight sites now construct the shared bar. Their object names, accessible
names and descriptions, tooltips, focus policies, checked state and
show/hide wiring are unchanged. `src/adapters/qt/AGENTS.md` now names the
shared bar as the way to make a chevron disclosure.

### Evidence

`qt_disclosure_button_scale_100` and `_200`
(`tests/adapters_qt/disclosure_button_tests.cpp`) construct each real host,
apply each built-in profile and read the bar closed, open, pressed and
hovered. Each bar is grabbed twice, with and without its title, so the
title's strongest pixel is measured against the bar beneath it. The checks:

- The title is at 4.5:1 or more.
- The bar is the theme's `Light`: Light `#FFFFFF` on `#F6F7F9`, Dark `#2A3039`
  on `#1C2026`, high contrast flat.
- The triangle is the text colour, and the arrow follows the state.
- A bar at its size hint draws the same title as a wide one.

Negative controls fail as follows:

- The old bar on the native bezel's grey under the theme's text fails the
  reading in all four themes. In Dark the title is 1.3:1, which is the
  owner's report. The shared bar under the same palette falls back to the
  surface and reads.
- The old bar as Fusion draws it fails in Light and Dark (fills `#E2E3E4` and
  `#22262B`, not the raised surface).
- With the old integer size hint, the elision check fails for Check details,
  Pull request and account, Technical details and Details.

The offscreen platform cannot draw the macOS style (it paints nothing), so the
bezel is reproduced from its grey (`#E3E3E3`). The new bar no longer
goes through the style, so its offscreen pixels are what macOS draws, apart
from font rasterisation.

Before and after, the old bar above the shared one on the theme's surface, at
200%:

- Light: [before-after-light.png](../../design/BPR-1579-disclosure-bars/before-after-light.png)
- Dark: [before-after-dark.png](../../design/BPR-1579-disclosure-bars/before-after-dark.png)
- In their hosts, Dark: [Check details](../../design/BPR-1579-disclosure-bars/after-overview-check-details-dark.png),
  [Requirement details](../../design/BPR-1579-disclosure-bars/after-overview-requirement-details-dark.png),
  [Pull request and account](../../design/BPR-1579-disclosure-bars/after-compose-review-details-dark.png);
  Light: [Check details](../../design/BPR-1579-disclosure-bars/after-overview-check-details-light.png)

The focused rows pass on macOS arm64 with Qt 6.11.2 offscreen:

- `qt_disclosure_button` at 100%, 200% (and run by hand at 125% and 150%)
- `qt_pull_request_overview_page` with its chips, folder-rows and read-marks
  rows
- `qt_review_submission_editor` at 100%, 125% and 200%
- `qt_source_search_panel` at both scales
- `qt_release_maintenance` at three scales
- `qt_settings_window` at four scales
- `qt_composed_github` at three scales, which walks the connection and
  technical disclosures by their arrow

clang-format 22 passes on every changed file. clang-tidy 22 with warnings as
errors passes on the new files. On the edited files its only finding is the
existing `using_` name at `server_card.cpp:738` (BPR-1373), on a line this card
does not touch. The documentation gate passes.

Documentation impact: presentation only. There is no core, host, persistence,
provider or command change, and no RFC acceptance is rewritten.
