---
schema: "implement-rfc.card.v1"
id: "BPR-1354"
title: "Every tooltip shows next to its control"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","origin":"BPR-1335 owner look, session b4b76c70c8e54f1933410b582189d0e8, BPR-1335-add-button-v002 Accept note","decision_id":"e7d697dd-056d-418f-bc6d-ffa10b11d687","reservation":"docs/issues/_reservations/BPR-1353--BPR-1354--7fb3f9709caa.json","related_cards":["BPR-1335","BPR-1328","BPR-1083"],"acceptance":["BPR-1354:inventory","BPR-1354:one-placement","BPR-1354:position","BPR-1354:control","BPR-1354:guard","BPR-1354:specification","BPR-1354:owner-look"]}
---

# BPR-1354: Every tooltip shows next to its control

## Owner note

On [[BPR-1335]]'s add-button v002 look (session
`b4b76c70c8e54f1933410b582189d0e8`, decision
`e7d697dd-056d-418f-bc6d-ffa10b11d687`, 2026-10-09T05:06:32Z) the owner
accepted the "+" with this note, verbatim:

> bug: Why is this damned tooltip far away again. This is a consistent problem.

The attached screenshot (kept in the session file, not committed) shows the
"+" on line 608 and its "Add comment (↩)" tooltip about three lines lower,
near line 611, and to the right of the button.

It is the third time. [[BPR-1083]] v004 ("The tooltip feels strangely far
away.") added the anchored tooltip for label marks, and [[BPR-1328]] ("Whys
this tooltip so far from the button?") anchored the title row and review bar.
Each fixed the surface the owner happened to be looking at; everything else
still used Qt's placement.

## Cause

`QToolTip` places its window at the pointer plus the platform cursor's
height (`QTipLabel::placeTip`: offset `(2, cursor height)`). On macOS that
offset follows the system cursor size, so with a larger pointer the tip
lands several lines below and to the right of whatever it names. Only
widgets that opted in, one by one, used the anchored tooltip; the "+"
(`diffLineAddButton`, a child of the head pane) never did.

## Scope

Every tooltip the Qt adapter shows, by one placement next to the control's
own rectangle: directly below it, left edges aligned, four pixels apart, or
above it where below would leave the screen.

- Widgets with their own `toolTip()`, anywhere in the application.
- Item views' per-item tooltips (`Qt::ToolTipRole`), tab bars' per-tab
  tooltips, header sections and menu actions: anchored to the item, tab,
  section or action rectangle.
- Painted controls and hover regions with no widget (gutter cells, scroll-bar
  ticks, marks, nodes on the epoch line): anchored to the painted rectangle.
- Programmatic tooltips (`QToolTip::showText`), including the info glyph's
  keyboard popover.

Out of scope: the system tray icon's tooltip, which the operating system
draws.

## Acceptance

- `BPR-1354:inventory`: the card lists every tooltip route found and what
  each now does.
- `BPR-1354:one-placement`: all of them go through one anchored placement;
  no `QToolTip::showText` remains outside it.
- `BPR-1354:position`: a test shows the tooltip's window directly below (or,
  at the screen's bottom edge, directly above) its anchor, left edges
  aligned, for the "+" and for at least two other kinds of control.
- `BPR-1354:control`: the same test fails against the code before the change.
- `BPR-1354:guard`: a repository check refuses a new `QToolTip::showText`
  outside the anchored placement.
- `BPR-1354:specification`: the specification states the rule for every
  tooltip, not only the title row and review bar.
- `BPR-1354:owner-look`: the owner accepts "tooltips appear next to their
  control" on a frozen build.
