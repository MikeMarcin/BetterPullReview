---
id: BPR-0116
title: Apply code line height and invalidate on a tab-width-only change
status: Backlog
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
