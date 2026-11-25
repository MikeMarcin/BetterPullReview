---
schema: "implement-rfc.card.v1"
id: "BPR-2383"
title: "The comments drawer's Only new and Hide resolved are dimmed when off and filled when on"
status: "Parked"
rfcs: []
depends_on: ["BPR-2372"]
priority: "P2"
platforms: ["mac", "win", "linux"]
parked_on: "human-review"
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner rejection of BPR-2372 in attended review 374cb080088e516421011d3a88d707e0, 2026-11-24","reservation":"docs/issues/_reservations/BPR-2383--BPR-2383--814cc2570d48.json","related":["BPR-1290","BPR-1337","BPR-2372"],"acceptance":["BPR-2383:faces","BPR-2383:state","BPR-2383:render-path","BPR-2383:neighbours"],"pending":["BPR-2383:owner-look"]}
---

# BPR-2383: The drawer's filter toggles are dimmed when off and filled when on

## Report

The owner, rejecting [[BPR-2372]] in attended review
374cb080088e516421011d3a88d707e0 on 2026-11-24: "it's not obvious when its on
or off still, there is a dimmed version for off from discuss look to that for
inspiration. Also your screenshot captures look busted, but the frozen build
looks better than those."

## Cause

Two faults in the face [[BPR-2372]] drew, both visible only in the
application's own themes:

- Off had no look of its own. It was the bare glyph in the whole ink, the
  same as the Previous and Next comment arrows beside it, so nothing said
  "this can be on and is not".
- On was nearly invisible. Its fill was the accent mixed toward the cards'
  base (`signal_soft`), but the drawer's ground is the window colour, and a
  theme's accent (`Highlight`) is its pale selection colour: `#7fb0e8` on
  `#f6f7f9` in Light, `#28518c` on `#1c2026` in Dark. The fill differed from
  the ground by a few levels and the one-pixel edge was pale in Light and
  dark-on-dark in Dark.

Discuss's chips ([[BPR-1337]]) do read: every chip has a hairline box, off is
a dimmed glyph and on is a filled box with the whole ink.

Three faults in the renders, none in the product:

- The test parented its reference `quiet_tool_button` into the filters' row
  without a layout position. It sat at the row's origin, over Only new, in
  both row renders.
- The row was grabbed out of a bare panel that was never shown, so it had
  the 100 by 30 pixel size of an unlaid widget, not the tab row the window
  composes.
- The renders were made on the offscreen platform with a palette made up for
  the test and the macOS style forced on the buttons. Offscreen is Fusion
  with another font database and cannot draw Cocoa, and the default
  palette's `Highlight` is a strong blue (`#308cc6`), so the renders showed
  a far plainer on state than any theme gives. Under the native look the
  same test binary never reached the filters: an earlier Discuss check fails
  there (see Remaining).

## Fix

- `discuss_tool`'s `native` shape (`src/adapters/qt/src/discuss_header.cpp`)
  is Discuss's chip at the drawer's size, on the drawer's own ground, drawn
  from the palette and never by the platform style. Off: an empty hairline
  box around the glyph dimmed until it reads at 4:1 on the drawer. On: the
  box filled from the ground toward the accent (26% in a light theme, 40% in
  a dark one) inside an edge in the accent held to 3:1 on the drawer
  (`focus_ring_colour`), the glyph in the whole ink. Under the pointer an
  off toggle shows a quiet grey face and the whole ink, never the accent.
  Keyboard focus draws the two-pixel ring on either state; a click draws
  none. A disabled filter is a bare glyph in the disabled ink.
- The toggle is a square the side of the style's icon tool. The two stand
  four pixels apart and four clear of the arrows
  (`src/adapters/qt/src/github_conversation_panel.cpp`).
- Tooltips still end ": on" or ": off" and the accessible names and checked
  state are unchanged from [[BPR-2372]].
- The test's render hook (`render_drawer_filters`,
  `tests/adapters_qt/github_conversation_panel_tests.cpp`) draws the
  `workbench_window` in Review with its Comments drawer open, in each
  built-in theme applied with `appearance_profile::apply_to`, each under the
  system appearance it is used in, and saves the tab row and the whole
  drawer in seven states, with Discuss's band in the same theme. It writes
  under a directory named for the platform and style that drew it
  (`cocoa-macos`, `offscreen-fusion`), so an offscreen render cannot pass
  for the owner's look. With `BPR_DRAWER_FILTER_RENDER_ROOT` set the binary
  runs only the renders and the filters' checks, so it runs under the native
  look. A window kept off screen is never made active, so the hook tells Qt
  it is before it draws keyboard focus.
- `src/adapters/qt/AGENTS.md` records the rule for a look of a control, and
  `docs/ux/workbench-specification.md` the toggles' look.

## Acceptance

- **BPR-2383:faces** `qt_github_conversation_panel`
  (`test_drawer_filters_read_off_when_off`), measured in the composed
  window in Light, Dark, High Contrast Light and High Contrast Dark, in the
  default style and with the macOS style set on the buttons. Each filter is
  a square, whole inside the tab row, clear of the other and of the arrows.
  Off: inside the box is the drawer's ground, the box is a hairline under
  3:1, and the glyph reads at 3:1 or more and under half the ink's contrast.
  On: the fill is at least 1.2:1 from the ground, the edge is at a mark's
  strength, the glyph is at least 4.5:1 on the fill, and the same glyph on
  and off differs by at least 3:1. Hovered, an off filter shows a face
  quieter than on's and the whole ink. Disabled, it is a bare glyph. Pass
  through CTest, and run directly at `QT_SCALE_FACTOR` 1, 1.25 and 2
  offscreen and at device pixel ratios 1, 1.25 and 2 under the native look
  (Cocoa, QMacStyle, `BPR_NATIVE_LOOK=1`).
  Negative control: against the old `discuss_header.cpp`,
  `discuss_header.hpp` and `github_conversation_panel.cpp` the same test
  fails, first at "each filter is a square the size of an icon tool"
  (offscreen) and "the boxes stand clear of each other and of the arrows"
  (native), and with those two checks put aside at "off, its box is a
  hairline, quieter than a mark" on both.
- **BPR-2383:state** same test: keyboard focus rings an off filter, which
  stays empty and dimmed, and an on one, which stays filled; a click's focus
  draws no ring; accessibility reports checkable, and checked when on and
  not when off; the tooltips read "Only threads with new replies: on/off"
  and "Hide resolved threads: off", also after `setChecked` under a
  `QSignalBlocker`.
- **BPR-2383:render-path** the hook's renders under the native look show the
  tab row as the window composes it (tabs, the conversation link, the count,
  the two filters, the arrows), with nothing overlapping. Device pixels, not
  rescaled.
- **BPR-2383:neighbours** `qt_workbench_window_scale_100/125/200`,
  `qt_composed_github_scale_100/125/200`,
  `qt_drawer_surfaces_fit_scale_100/125/200` (the wider row still fits the
  280-pixel drawer and never pans), `qt_button_faces`,
  `qt_button_faces_cocoa` and `qt_view_filter_memory_window`: pass.
  clang-format 22 check and clang-tidy with warnings as errors on
  `discuss_header.cpp`, `github_conversation_panel.cpp` and
  `github_conversation_panel_tests.cpp`: clean.
  `python3 scripts/verify_documentation.py --root .`: pass.

Evidence revision: c02bd5044.

Renders (native look, not committed): before and after of the tab row and the
drawer, four themes, device pixel ratios 1, 1.25 and 2, with contact sheets.

## Pending

- **BPR-2383:owner-look** whether on and off are now unmistakable on the
  owner's display. A `build` question for a frozen build of the integrated
  head is prepared (wording passes `attended_review.py check`); it needs
  this card's ID, the freeze's launcher and its `SESSION_LINE`.

## Remaining

- Under the native look the rest of `qt_github_conversation_panel` does not
  pass: `test_discuss_top_is_one_named_band` fails at "one 30-pixel row of
  chips in a band under 50 pixels". It fails the same way without this
  change, CTest runs the row offscreen, and it is outside this card.
- `agentActivitySummary` is still a plain checkable `QToolButton`, as
  [[BPR-2372]] left it for the agent panel's redesign.
