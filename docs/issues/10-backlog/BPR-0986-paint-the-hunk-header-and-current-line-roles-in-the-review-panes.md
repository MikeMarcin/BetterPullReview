---
schema: "implement-rfc.card.v1"
id: "BPR-0986"
title: "Paint the hunk-header and current-line roles in the review panes"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"BPR-0790, recorded as drift for main to allocate; allocated at RFC-BPR-0034 closeout BPR-0791","found_at":"a5427499","rechecked_at":"f06088b8","origin_rfc":"RFC-BPR-0034","disposition":"standalone follow-up; not an RFC-BPR-0034 closeout dependency; RFC-BPR-0006 is not reopened"}
---

# BPR-0986: Paint the hunk-header and current-line roles in the review panes

## Defect

The Appearance page lists all 47 semantic roles and lets a person customise
each one. Four things on that page have no counterpart in the review window:

- `diff_hunk_header_background` and `diff_hunk_header_text` are painted by
  nothing at all. A person can change them, and the change shows on no
  surface.
- `code_current_line_background` and the tab markers are painted only by the
  Appearance page's preview (`code_sample_painter`, used by `type_preview` and
  `theme_preview_strip`). The review panes paint neither, so the preview shows
  the person something the review window will not.

[[BPR-0790]] found this when it lifted the review window's role-to-format,
row-band and byte-to-character helpers into the shared `code_sample_painter`
translation unit. It deliberately did not widen what the review pane paints,
and recorded the gap as drift for main to allocate. [[BPR-0791]] carried it
unchanged into RFC-BPR-0034's C4 row. The owner accepted the rendered page on
2026-09-16. That verdict covers the page as shipped; it does not cover this
gap.

Rechecked at `f06088b8`. Outside the appearance profile and the role display,
no Qt source names either hunk-header role. `code_current_line_background` and
`paint_tab_markers` are reached only through `code_sample_painter`, and the
review panes use only its shared helpers.

## Scope

- Decide per role whether the review pane paints it or the role's contract
  changes. Hunk headers may already be drawn as a boundary rule ([[BPR-0202]])
  rather than a fill. In that case, record which role that rule consumes, and
  don't paint a second band beside it.
- If the review pane paints the current line and tab markers, use the shared
  helpers the preview already uses. RFC-BPR-0034 forbids a painter of the
  preview's own, and the same rule applies in reverse: no second renderer in
  the pane.
- If a role stays unpainted, the Appearance page must stop offering it as
  though it did something. Removing a role or changing a theme document is
  RFC-BPR-0006's contract, and doing either needs its own decision.

## Acceptance

- Every role the Colours card offers changes at least one review-window
  surface, or the card no longer offers it, and the decision is recorded.
- The review panes paint the current line and tab markers with the colours the
  preview shows, or the preview stops showing them.
- `qt_syntax_pane_composition`, `qt_diff_workbench_scale_100/125/200`,
  `qt_type_preview`, `qt_code_sample_painter` and `qt_theme_preview_strip`
  pass, and a rendered pane capture shows the changed roles.

## Boundaries

The core decides, the host performs, Qt presents. No stylesheet or hard-coded
palette. No new roles or modes without an RFC-BPR-0006 decision.
