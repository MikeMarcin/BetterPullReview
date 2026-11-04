---
schema: "implement-rfc.card.v1"
id: "BPR-1855"
title: "Windows sign-in buttons clip their text"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"Owner, 2026-11-04: \"Windows startup screen is clipping text\" (phone photo of the signed-out Home card: \"Ise GitHub Cl\", \"ıdd enterprise serve\", and the sentence past the right edge)","related_cards":["BPR-0646","BPR-0775","BPR-1577","BPR-1659","BPR-1660"],"related_rfcs":["RFC-BPR-0087"],"id_allocation":"Reserved on main (8a1f897e4).","frontend_design":"frontend-design loaded. No new look: the accepted chip keeps its padding, surface and type; only who owns the content geometry changes."}
acceptance: [{"criterion":"AC1","outcome":"pending"},{"criterion":"AC2","outcome":"pending"},{"criterion":"AC3","outcome":"pending"},{"criterion":"AC4","outcome":"pending"},{"criterion":"AC5","outcome":"pending"}]
---

# BPR-1855: Windows sign-in buttons clip their text

## Goal

On Windows the signed-out Home card showed "Use GitHub CLI" as "se GitHub
CL" and "Add enterprise server" as "dd enterprise serve": the first and last
letters were cut off inside the button's border. Every bordered secondary
button must be as wide as its words in the font and at the scale it is
shown, and draw them whole. The card's sentence must wrap inside the card.

## Cause

Use GitHub CLI and Add enterprise server are `quiet_push_button`s
(RFC-BPR-0087's shared chip). The chip measured itself as its words plus six
pixels a side, then handed the inner rectangle to the style's
`CE_PushButtonLabel`. Qt's `windows11` style, the default on Windows 11,
takes a further `contentHMargin` of six pixels from each side of that
rectangle before drawing a centred label (`qwindows11style.cpp`, Qt 6.11).
The words were left twelve pixels narrower than themselves, so a centred
draw lost about one Segoe UI letter at each end. macOS and Fusion keep no
such margin, which is why it never showed there. `quiet_tool_button` had
the same split: its size came from integer metrics and its label from the
style, which elides when the two disagree.

The sentence was already a word-wrapping label in the card's layout; it is
covered by the same test so a regression would be seen.

## Design

- `button_surface` owns the chip's content geometry. `chip_content_size`
  measures the words with `QFontMetricsF`, rounded up, plus the icon and its
  gap when shown. `paint_button_label` centres icon and words inside the
  chip's six-pixel padding and draws the words through the style's
  `drawItemText`, so the platform still decides mnemonic underlining and
  text rendering, but no style label element can inset or elide them.
- `quiet_push_button` and `quiet_tool_button` size from
  `chip_content_size` and paint through `paint_button_label`. No widths are
  fixed: a different font, display scale or translation changes the measured
  size.
- Native fallbacks (default, menu, icon-only) are unchanged; the primary
  Sign in is a native `QPushButton` whose style both sizes and draws it.

## Acceptance

- **AC1**: Under a style that keeps Windows 11's six-pixel label margin and a
  wide typeface (Segoe UI where installed, else Verdana), each of Sign in, Use
  GitHub CLI and Add enterprise server, and a long-text `quiet_push_button`
  and `quiet_tool_button`, has a `sizeHint` at least its words plus padding,
  is laid out at that width, and draws its words unelided in a rectangle at
  least as wide as them and inside the button, at 100, 125, 150 and 200%
  scale, wide and at the card's narrowest.
- **AC2**: The card's sentence wraps, ends inside the card and has the height
  its lines need at each scale; at the narrowest it takes several lines.
- **AC3**: Offscreen renders of the card at the four scales show the words
  whole.
- **AC4**: Existing quiet button contract and production surfaces still pass;
  format, tidy and documentation gates pass.
- **AC5**: The same test passes on Windows with Qt's own `windows11` style
  and Segoe UI at the four scales, and its renders show the words whole.

## Not covered

- `epoch_comparison_control`'s Comparing button sets a fixed width, but from
  the style's own `sizeFromContents` with an explicit elide; it is not this
  pattern and was left.
