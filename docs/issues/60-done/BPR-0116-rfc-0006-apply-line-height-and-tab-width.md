---
id: BPR-0116
title: Apply code line height and invalidate on a tab-width-only change
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0107]
---

# Apply code line height and invalidate on a tab-width-only change

BPR-0107 delivered UI and code typography but left two settings short of the
RFC's criterion that line height and tab width "persist and preview
immediately". Both shortfalls were reported rather than papered over, and both
need files outside that slice's scope.

**Code line height is computed but never applied.** `code_line_height()`
returns the correct derived measurement and is unit-tested, but no surface
consumes it. Qt's `QPlainTextDocumentLayout` ignores
`QTextBlockFormat::lineHeight`, so honouring the setting requires the surface
that owns the document — `diff_workbench` and its source panes. The delivering
slice deliberately did not fake-apply it, which was the right call: a setting
that appears to apply but does not is worse than one that visibly does not,
because it defeats the next reader's attempt to find the bug.

**A tab-width-only change does not invalidate measured geometry.**
`apply_code_font_to` sets `tabStopDistance` and orders it before the font, so a
combined change invalidates correctly. But `QWidget::setFont` emits no event
when the font itself is unchanged, and the workbench listens only for font,
application-font, screen, and viewport facts. `measured_geometry_change::tab_width`
exists in the enumeration and is currently unreachable. Changing only the tab
width therefore leaves ribbons and the measured axis stale against text that
has visibly moved.

Close both by giving the workbench a typography publication to subscribe to,
rather than by widening what a font change means. Tab width is not a font
property, and inventing a synthetic font change to force invalidation would
make the reason for the rebuild untrue.

## Required direct evidence

- A line-height change visibly alters code line spacing on the composed
  surface, and the applied value matches `code_line_height()`.
- A tab-width-only change, with every font property held constant, invalidates
  the measured axis and ribbon geometry while leaving mapping and confidence
  byte-identical — the same invariant BPR-0107 proved for a font change.
- `measured_geometry_change::tab_width` is reachable, or is removed if the
  chosen design makes it genuinely redundant.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## Outcome

The tab-width half is closed. The line-height half is **not**, and the reason
is measured rather than assumed: it is a Qt constraint that trades against a
guarantee RFC-BPR-0005 already proved. It moves to BPR-0118 with the
measurements below.

### Tab width — closed

`appearance_profile` now publishes the applied code typography as its own fact,
a `code_typography_event` carrying the applied tab stop and line height, sent to
the surface after the fonts are applied. `diff_workbench` subscribes and
invalidates on a tab stop that actually moved.

This follows the card's instruction not to widen what a font change means. The
event carries values rather than a bare notification, so the workbench can tell
which fact moved: a combined change already arrives as a font change, and
rebuilding again on the publication would double the work and attribute it to
the wrong fact. Re-publishing unchanged typography therefore invalidates
nothing, and the first publication is adopted as a baseline rather than
mistaken for a change.

`measured_geometry_change::tab_width` is now reachable from production code;
before this slice it appeared only in a test.

`test_a_tab_width_change_alone_invalidates_measured_geometry` holds every font
property constant — the assertion `left_pane()->font() == beforePaneFont` is
what makes the claim mean anything — and proves the measured axis and ribbon
geometry rebuild while mapping, pairing confidence, counterpart, detail,
selection, and the painted ribbon identity stay byte-identical.

Both new behaviours were broken separately and observed to fail for their own
reason: removing the invalidation fails "the tab width alone invalidates the
measured axis", and removing the unchanged-value guard fails "re-publishing an
unchanged tab width invalidates nothing".

### Line height — measured to be out of reach here

`QPlainTextDocumentLayout` ignores line height in **every** mode Qt offers. A
probe on this host, Qt 6.11.1, built a `QPlainTextEdit` over a three-line
document and measured `blockBoundingRect().height()` for the first block:

| Applied to every block | Measured block height |
|---|---|
| nothing (baseline) | 19 |
| `ProportionalHeight` 200 | 19 |
| `FixedHeight` 40 | 19 |
| `MinimumHeight` 40 | 19 |
| `LineDistanceHeight` 40 | 19 |
| `topMargin` 7 and `bottomMargin` 5 | 19 |

Block margins are ignored too, so the usual workaround is unavailable. The
original card said honouring the setting "requires the surface that owns the
document"; the measurement narrows that further — it requires replacing the
panes' text-rendering strategy, either with `QTextEdit` or with a custom
`QAbstractTextDocumentLayout`.

That is not a coding gap but a trade against an already-proven claim.
RFC-BPR-0005's virtualization evidence rests on `QPlainTextEdit`, which lays out
only visible blocks; it records 68 window widgets at 60, 6,000, and 60,000
lines. `QTextEdit`'s `QTextDocumentLayout` lays out the whole document, so
adopting it would put the bounded-widget and large-file figures at risk to
satisfy one typography setting. Faking the setting was already rejected by the
delivering slice for the right reason, and that judgement stands.

`measured_geometry_change::line_spacing` therefore stays unreachable from
production. It is deliberately **not** removed: it becomes reachable the moment
line height is applied, and BPR-0118 owns that decision.
