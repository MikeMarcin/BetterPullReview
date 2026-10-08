---
schema: "implement-rfc.card.v1"
id: "BPR-1336"
title: "Zoom the code in the comparison: View > Zoom In, Zoom Out and Actual Size"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner chat request, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1336--BPR-1336--e0eddadebca1.json","related_cards":["BPR-1334","BPR-1335","BPR-1328","BPR-1295","BPR-1107","BPR-1291","BPR-0118"],"requested_skill":"frontend-design","acceptance":["BPR-1336:C1","BPR-1336:C2","BPR-1336:C3","BPR-1336:C4","BPR-1336:C5","BPR-1336:C6"]}
---

# BPR-1336: Zoom the code in the comparison

## Owner request

The owner, in chat on 2026-10-08, verbatim:

> I think we should have a more accessible zoom/text size for the diff pane

## What exists today (inventory, `origin/main` a8f4531e8)

- **Code size lives in Settings only.** Settings > Appearance > Type has a
  **Code size** row (`appearanceCodeSizeRow`, a point spin box bounded by
  `core::bounds_of(point_size)`, 6 to 72 pt). It is one field of
  `core::code_font_settings`, persisted per profile in the appearance store
  (schema version 8) and applied to every code surface: the panes, the
  conversation cards, the finder, Overview's reader and the share sheet.
  Reaching it takes the Settings window, a scroll to Type and a spin box.
- **No zoom exists.** No action, key, wheel or gesture changes the code size.
  `measured_geometry_change::zoom` is declared and nothing raises it.
  `docs/ux/appearance-and-accessibility.md` already promises that "runtime code
  zoom preserves the top visible source line and does not silently overwrite
  saved font settings", with nothing behind it.
- **⌘-scroll scrolls.** The comparison's scroll controller handles every wheel
  event over the panes and ignores Control/Command, so ⌘-scroll is a plain
  scroll; `QPlainTextEdit`'s own Ctrl-wheel zoom never runs. Pinch is not
  handled.
- **How the comparison scales with the code font.**
  `appearance_profile::apply_code_font_to(comparison)` sets the font on the
  comparison and every `QPlainTextEdit` in it (both panes, the one-pane
  document and the Detail pane), sets their tab stops, and publishes the tab
  stop. Each pane's gutter re-widths on `FontChange` from its digit count, so
  the line numbers already follow the font. The connector and link map are
  painted from measured geometry, which the font change invalidates and
  rebuilds over bounded turns (512 lines or 4 ms a turn). The scroll
  controller keeps the line on its synchronization rail, 43% down the
  viewport, across a remeasurement ([[BPR-1291]]).
- **Line details.** [[BPR-1334]] (in progress, not on `origin/main`) turns the
  Detail pane into the Line details strip. The Detail pane is a
  `QPlainTextEdit` inside the comparison, so it takes the comparison's code
  font with the panes.
- **The drawer's code snippets** take `appearance_profile::code_font()` through
  `adopt_appearance`, separately from the comparison.
- **Keys.** ⌘0 (Ctrl+0) is Home; inside the open label menu it is Plain
  ([[BPR-1080]]). ⌘= ⌘+ ⌘− and ⌘⌥0 are unbound in the window, its desktop
  menu bar and the comparison control (which holds Ctrl+Alt with arrows, R, P,
  Shift+D and Escape).

## Decision: a zoom multiplier, beside Code size

Zoom is a percentage, **Diff zoom**, stored per profile with the code font and
shown in Settings > Appearance > Type directly under Code size. It does not
change Code size.

- **Actual Size has a meaning.** With a multiplier, Actual Size returns to the
  reviewer's own Code size. If zoom rewrote Code size, Actual Size could only
  return to the factory size and would throw away a size the reviewer chose.
- **It keeps the existing contract.** The appearance spec already says runtime
  zoom does not overwrite saved font settings.
- **It is scoped to the comparison.** Drawers, Overview and the composer stay at
  Code size, so zooming the diff to read a long line does not also reflow the
  conversation column.
- **It persists like Code size.** The profile's appearance record carries it
  (schema 9, a version-8 record takes 100%). A restart, a second window and
  Settings all show the same value, and the Type card's Reset resets it with
  the code font.

## Change

- **View > Zoom In, Zoom Out, Actual Size**, in the window's View menu and the
  resident desktop menu bar (disabled there, per [[BPR-0796]]).
  - Zoom In: ⌘= and ⌘+ (Ctrl+= and Ctrl++).
  - Zoom Out: ⌘− (Ctrl+−).
  - Actual Size: **⌘⌥0 (Ctrl+Alt+0)**. ⌘0 stays Home, and Plain inside the
    label menu. ⌘⌥ is the modifier pair the comparison control already uses
    for its own commands, and the label menu matches ⌘0 exactly, so ⌘⌥0
    never chooses Plain. On Windows, where AltGr arrives as Ctrl+Alt, an
    editable text field that receives AltGr+0 as a typed character (`}` on
    German and Nordic layouts) keeps the character.
- **⌘-scroll and pinch** over the code panes zoom by one step for each wheel
  notch, for each 60 px of trackpad scroll, or for each 0.15 of pinch.
- **Steps** are 50, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200, 250 and 300
  percent. Core owns the steps, the clamp and the zoomed size. The zoomed size
  is the Code size times the percentage, held to the 6 to 72 pt bounds. Zoom In
  is disabled when the next step would not change the size, and Zoom Out at
  the bottom. Each disabled command says why.
- **What zooms:** both panes and the one-pane document, their gutters (which
  re-width to the digits), the connector and link map (remeasured), the tab
  stops, and the Detail pane / Line details strip. The drawer's snippets keep
  Code size.
- **Indicator:** a small pill at the top centre of the comparison shows the
  percentage and the size on screen ("125%  16.3 pt") for 1.2 s, in the theme's
  own surface and text colours, with no fade under reduced motion. The same
  words are a polite accessibility announcement.
- **Scroll anchor:** a visible current line stays where it was in the viewport.
  Otherwise the line on the rail stays, as for any remeasurement.

## Acceptance

- **C1 (direct).** In the production window, View > Zoom In, Zoom Out and Actual
  Size and their keys step the comparison's code font through the core steps,
  and stop at the clamps with the reason shown. The panes, the gutter (wider
  for the digits), the tab stop and the Detail pane all follow. Code size is
  unchanged, and the drawer's snippets keep Code size.
- **C2 (direct).** Keys do not collide. Each zoom key is bound once in the
  assembled window and in the desktop menu bar. ⌘0 still opens Home, and ⌘0
  inside the label menu still chooses Plain while ⌘⌥0 there does not. The
  AltGr claim keeps a typed `}` in an editor. ⌘-scroll and pinch step the
  zoom, and a plain scroll still scrolls.
- **C3 (direct).** Zoom persists per profile through the appearance host and
  store: a version-8 store migrates to 100%, a zoomed profile reopens zoomed,
  Settings shows and sets the same value, and Type's Reset returns it to 100%.
- **C4 (direct).** A visible current line keeps its viewport position across a
  zoom step, and the indicator and the announcement report the new
  percentage and size.
- **C5 (direct).** Zoom In, Zoom Out and Actual Size on the 20,000-line large
  file in the optimized large-review benchmark each stay within the 100 ms
  input target.
- **C6 (owner).** The owner looks at a frozen build and answers Accept or Reject
  for "diff zoom".
