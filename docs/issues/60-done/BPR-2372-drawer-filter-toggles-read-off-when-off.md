---
schema: "implement-rfc.card.v1"
id: "BPR-2372"
title: "The comments drawer's Only new and Hide resolved read off when off"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner report with photo, 2026-11-24","reservation":"docs/issues/_reservations/BPR-2372--BPR-2372--bda469312156.json","related":["BPR-1290","BPR-1337","BPR-1579"],"acceptance":["BPR-2372:faces","BPR-2372:state"]}
---

# BPR-2372: The comments drawer's filter toggles read off when off

## Report

The owner, 2026-11-24, with a photo of the comments drawer beside the code:
"These filter buttons at the top of the comments drawer look enabled to me
even when they're off like in the photo here". Both icons (Only new, Hide
resolved) were off, yet each sat on a filled light grey square that read as
pressed. The Previous and Next comment arrows beside them were bare.

## Cause

Beside the code the two filters are `discuss_tool`s in the `native` shape
([[BPR-1290]], [[BPR-1337]]). That shape fell through to
`QToolButton::paintEvent`, so the platform style drew the face. Outside a
toolbar, QMacStyle's `CC_ToolButton` ORs `State_Sunken` into the button's
flags whenever `SC_ToolButton` is a subcontrol, which is always. It then
draws an `NSBezelStyleShadowlessSquare` bezel under every tool button,
whether `autoRaise` is set and whether the button is checked. Checked
changes the bezel's state only while the window is active. So off and on
drew the same grey square. `quiet_tool_button` (the arrows) already paints
its own face for this reason, as [[BPR-1579]]'s disclosures do; the
drawer's filters were the remaining checkable icon buttons left to the
platform style. Under Fusion a checked filter drew a grey panel and keyboard
focus drew no ring.

## Fix

- `discuss_tool` in the `native` shape draws its own face from the palette
  and never asks the style. Off is the quiet icon tool the arrows are: no
  face, a hover tint, the glyph in the window's ink. On is a soft accent
  fill (the chosen chip's `signal_soft`) inside a one-pixel accent edge, as
  the toolbar's chosen toggles. Keyboard focus draws the app's two-pixel
  focus ring, on or off; a click leaves none.
- `discuss_tool::set_toggle_tip`: the filter's tooltip ends with ": on" or
  ": off". `checkStateSet` keeps it current, including when a remembered
  filter is set with signals blocked. Accessible names ("Only new", "Hide
  resolved") are unchanged; Qt reports the checkable and checked state.

Other checkable icon buttons: `agentActivitySummary` (`agent_run_status.cpp`)
is a plain checkable auto-raise `QToolButton` outside a toolbar and gets the
same macOS bezel in both states. Its arrow still shows open or closed. It
was left alone because the agent panel is being redesigned separately. The
review bar's Seen and Read steps are drawn by `strip_style`, and every other
checkable tool button is a `quiet_tool_button`, a chip subclass or a strip
button.

## Acceptance

- **BPR-2372:faces** `qt_github_conversation_panel`
  (`test_drawer_filters_read_off_when_off`), in the default offscreen style
  and with the macOS style set on the buttons, light and dark palettes. Off,
  the filter's image equals a `quiet_tool_button` icon tool's image pixel
  for pixel, and its edge and inside are the window ground. On differs from
  off, fills with an accent-leaning face and has an edge off the ground;
  the glyph's ink holds. Keyboard focus rings an off filter and an on one.
  Run directly at `QT_SCALE_FACTOR` 1, 1.25 and 2: pass. Against the old
  `discuss_header.cpp`, `discuss_header.hpp` and
  `github_conversation_panel.cpp` the same test fails (macOS style: off is
  not the quiet tool, and on and off draw identically).
- **BPR-2372:state** same test: the accessible interface reports checkable,
  and checked when on and not when off; the tooltips read "Only threads with
  new replies: on/off" and "Hide resolved threads: off", also after
  `setChecked` under a `QSignalBlocker`.

Evidence revision: 511d8f247.

Renders (offscreen, not committed): before and after, 100/125/200%, light
and dark. Offscreen QPA cannot draw Cocoa's bezel, so the macOS-style
"before" panels are blank rather than showing the owner's grey square; the
bezel is established from QMacStyle's source and the owner's photo.

Owner look: whether on reads clearly enough on the real display (soft accent
fill and accent edge, as the toolbar's split and panel toggles).
