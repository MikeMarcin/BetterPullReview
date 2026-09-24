# BPR-0986: the review panes' current-line band and tab markers, rendered

Rendered on the Mac integrator (macOS 26.6.2 arm64, Qt 6.11.2, offscreen,
`build/dev`) from the change in [[BPR-0986]]. The comparison is the same one in
every capture: an eight-line C function indented with tabs, one replaced line
(`return one;` to `return one + 1;`), and the reader on line 4 of the right
side (`if (one) {`), selected by `select_conversation_line`. That is an
unchanged line, so it is the case where the current-line band shows.

| Capture | What it shows |
|---|---|
| [panes-light-before.png](BPR-0986-review-pane-roles/panes-light-before.png) | Light, before: no tab markers; line 4 washed alike on both sides |
| [panes-light-after.png](BPR-0986-review-pane-roles/panes-light-after.png) | Light, after: a marker per leading tab on both sides; right line 4 wears the current-line band under the wash |
| [panes-dark-before.png](BPR-0986-review-pane-roles/panes-dark-before.png) | Dark, before |
| [panes-dark-after.png](BPR-0986-review-pane-roles/panes-dark-after.png) | Dark, after |
| [preview-light.png](BPR-0986-review-pane-roles/preview-light.png) | The Appearance page's type preview in light, for comparison (unchanged by this card) |
| [preview-dark.png](BPR-0986-review-pane-roles/preview-dark.png) | The same preview in dark |

The "before" captures come from the same source with the two paint additions
switched off. The "after" captures come from the change as committed.

Pixels read from the captures, at the middle of each row:

| Theme | Right line 4 (reading, washed) | Left line 4 (its washed counterpart) | Right line 3 (unchanged) |
|---|---|---|---|
| Light, before | (219, 233, 249) | (219, 233, 249) | (255, 255, 255) |
| Light, after | (210, 227, 247) | (219, 233, 249) | (255, 255, 255) |
| Dark, before | (20, 35, 57) | (20, 35, 57) | (13, 17, 23) |
| Dark, after | (27, 43, 66) | (20, 35, 57) | (13, 17, 23) |

The reading line is always selected in the panes, so its band is always under
the selection wash. In the preview, the current row is a separate unselected
row. In the panes the band makes the reader's line a shade apart from the rest
of the washed selection and leaves every other line as it was. The tab markers
are the preview's own: the shared `paint_tab_markers` helper in
`code_line_number_text`, at the pane's tab stop.

These captures are evidence for the owner's review of the change. They are
not an acceptance of it.

## The Colours card offers 45 roles (2026-09-24, owner decision `e16dfcbf`)

Rendered on the Mac integrator (macOS 26.6.2 arm64, Qt 6.11.2, offscreen,
`build/dev`) through `colour_role_list`, the Colours card's own list, with the
built-in light or dark palette applied and the filter set to `Diff`. The
"before" captures come from the same source with the two hunk-header roles
offered again; the "after" captures come from the change as committed.

| Capture | What it shows |
|---|---|
| [colours-diff-light-before.png](BPR-0986-review-pane-roles/colours-diff-light-before.png) | Light, before: Diff 8, including "Diff hunk header background" and "Diff hunk header text" |
| [colours-diff-light-after.png](BPR-0986-review-pane-roles/colours-diff-light-after.png) | Light, after: Diff 6; the two hunk-header rows are gone |
| [colours-diff-dark-before.png](BPR-0986-review-pane-roles/colours-diff-dark-before.png) | Dark, before |
| [colours-diff-dark-after.png](BPR-0986-review-pane-roles/colours-diff-dark-after.png) | Dark, after |

With the filter cleared, the list holds 47 rows before and 45 after. The
theme documents, import and export keep both roles; only the card stops
offering them.
