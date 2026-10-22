---
schema: "implement-rfc.card.v1"
id: "BPR-1734"
title: "A full-width text selection repaints to the pane's edge when it clears or loses focus"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1734--BPR-1734--cbbb2c8cced1.json","kind":"bug","origin":"owner note on RFC-BPR-0090's one-pane look, 2026-10-22 (session 7dba4a1f1dbf08a0ad93da313b35fbcd, decision b01228b2), relayed by the coordinator","introduced_by":"d7d95ed0d (BPR-1533, 2026-10-15)","related_cards":["BPR-1533","BPR-1729"]}
acceptance: [{"criterion": "AC1: With full-width selections, losing focus and clearing a selection over lines 1 to 13 repaint past the longest line to the pane's edge, in one pane and in split view; the course fails without the fix", "outcome": "pass", "revision": "ad9dbb5f1260ef6f3dc2f27f1d42d05128501799", "evidence": {"kind": "command", "command": "QT_QPA_PLATFORM=offscreen ctest -R '^qt_workbench_window_scale_(100|125|200)$'", "detail": "test_a_full_width_selection_repaints_to_the_edge gives the head pane a proxy style with SH_RichText_FullWidthSelection, widens the window to 2400x900, drags over lines 1 to 13, and checks that losing focus to the conversation panel and clearing the selection each repaint a point 8 px from the viewport's right edge on line 4, in one pane and in split view. Before the fix the course failed at 'one pane: losing focus repaints the selection past the longest line, to the pane's edge'; with the fix forced off (negative control) it failed the same way; with the fix it passes at 100, 125 and 200 percent."}}, {"criterion": "AC2: The related suites, format, tidy on the changed files, the reseal and the documentation gate pass", "outcome": "pass", "revision": "ad9dbb5f1260ef6f3dc2f27f1d42d05128501799", "evidence": {"kind": "command", "command": "QT_QPA_PLATFORM=offscreen ctest -R '^qt_(workbench_|diff_workbench_scale_)'; format_check.py; tidy.py --warnings-as-errors; reseal_command_correctness.py; reseal_round_trip_composition.py --check; verify_documentation.py", "detail": "The 24 qt_workbench_* and qt_diff_workbench_scale_* suites pass. clang-format 22.1.8 check passes. clang-tidy passes on diff_workbench.cpp; in workbench_window_tests.cpp it reports only the two std::endl lines in the case tracer from b059676cc7, outside this change. The window tests' pin and the --line-draft inventory were resealed; the round-trip pins are unchanged. The documentation gate fails only on this card's provisional ID."}}]
---

# BPR-1734: A full-width text selection repaints to the pane's edge when it clears or loses focus

## Owner report

On RFC-BPR-0090's one-pane look (2026-10-22), verbatim: "One-pane seems to
have an unreleated virtual space highlighting problem. The attached
screenshot was after i made a selection and hit esc outside of the suggest
mode." The screenshot shows, in the dark theme, a blue block from just past
the longest line (`void Renderer::resize(const int aWidth, const int
aHeight)`) to the pane's right edge, over the rows the selection had
covered, with the text area itself no longer highlighted.

## Root cause

[[BPR-1533]] (`d7d95ed0d`, 2026-10-15) paints the comparison panes' text
selection itself, solid and on top. It keeps the style's choice: with the
style macOS uses (`SH_RichText_FullWidthSelection`), a selection that runs
past a line's end is painted to the pane's edge, past every line. When the
selection changes, or the pane loses focus and the selection turns the
inactive colour, Qt invalidates only the selection's rectangle in document
coordinates, whose right edge is the document's width: the longest line. The
part from there to the pane's edge was never repainted and kept the old
fill. It is not specific to one pane or to RFC-BPR-0090: the split panes
paint the same way. Offscreen the fault reproduces once the pane is given
that style hint. Esc itself clears nothing in a pane; on the owner's machine
it most likely moved focus out of the pane (the old fill there is the active
blue and the text area shows no selection), which the course covers as
"losing focus", alongside clearing the selection.

## Fix

`source_pane` repaints the rows the last and the current selection cover,
across the whole viewport, when the selection changes and when focus enters
or leaves, but only while the style paints selections to the edge. The work
stays bounded by the visible rows.

## Acceptance criteria

- AC1: With full-width selections, losing focus and clearing a selection over
  lines 1 to 13 repaint past the longest line to the pane's edge, in one pane
  and in split view; the course fails without the fix.
- AC2: The related suites, format, tidy on the changed files, the reseal and
  the documentation gate pass.

## Result

Landed in `ad9dbb5f1`. The provisional ID is the coordinator's to allocate.
