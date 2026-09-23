---
schema: "implement-rfc.card.v1"
id: "BPR-0986"
title: "Paint the hunk-header and current-line roles in the review panes"
status: "Parked"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"BPR-0790, recorded as drift for main to allocate; allocated at RFC-BPR-0034 closeout BPR-0791","found_at":"a5427499","rechecked_at":"f06088b8","origin_rfc":"RFC-BPR-0034","disposition":"standalone follow-up; not an RFC-BPR-0034 closeout dependency; RFC-BPR-0006 is not reopened"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1 Every role the Colours card offers changes at least one review-window surface, or the card no longer offers it, and the decision is recorded","outcome":"pending","revision":"55e9405f","evidence":{"kind":"observation","detail":"Tab markers and code_current_line_background are now painted by the review panes (and the role already reached the Overview side column and Home landing through AlternateBase). diff_hunk_header_background and diff_hunk_header_text have no review-window surface: the panes have no hunk header and draw a group's start as a rule in diff_group_boundary_rule. Dropping them from the card changes RFC-BPR-0034 C5's accepted 47 roles and retiring them is RFC-BPR-0006's, so the choice is the owner's (ask 1 on the card).","path":"docs/issues/40-parked/BPR-0986-paint-the-hunk-header-and-current-line-roles-in-the-review-panes.md#owner-asks--parked-on-human-review"}},{"criterion":"AC2 The review panes paint the current line and tab markers with the colours the preview shows, or the preview stops showing them","outcome":"pass","revision":"55e9405f","evidence":{"kind":"command","detail":"The panes paint tab markers with the shared paint_tab_markers helper in code_line_number_text at their own tab stop, and the unchanged reading line with code_sample_row_fill(current_line), the preview's rule, beneath the selection wash, in both layouts. qt_diff_workbench_scale_100/125/200 require both, and each fails with its paint call switched off.","path":"docs/issues/40-parked/BPR-0986-paint-the-hunk-header-and-current-line-roles-in-the-review-panes.md#evidence"}},{"criterion":"AC3 qt_syntax_pane_composition, qt_diff_workbench_scale_100/125/200, qt_type_preview, qt_code_sample_painter and qt_theme_preview_strip pass, and a rendered pane capture shows the changed roles","outcome":"pass","revision":"55e9405f","evidence":{"kind":"artifact","detail":"All seven pass, with the appearance, workbench-window and large-review neighbours. Before and after captures in light and dark, with pixel readings, are in docs/verification/BPR-0986-review-pane-roles.md.","path":"docs/verification/BPR-0986-review-pane-roles.md"}},{"criterion":"Owner visual acceptance of the rendered review-pane change","outcome":"pending","revision":"55e9405f","evidence":{"kind":"artifact","detail":"Not self-certified. Ask 2 on the card: whether tab markers on every leading tab, and the reader's line a shade apart under the selection wash, are accepted as the review window's look, judged from the after/before captures.","path":"docs/verification/BPR-0986-review-pane-roles.md"}}]
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

## Resolution — 2026-09-23, Mac integrator (Mac batch 2)

### Decisions, per role

- **Tab markers: the review panes paint them.** `source_pane::paintEvent` marks
  each visible line's leading tabs with the shared `paint_tab_markers` helper,
  in `code_line_number_text`, at the pane's own tab stop. The preview makes the
  same call with the same role, and RFC-BPR-0034 C4 says the preview is "the
  review window's painter in miniature". The markers therefore follow the Tab
  width setting in the panes as they do in the preview. They apply to both
  panes and the one pane, and only the blocks the viewport shows are walked.
- **`code_current_line_background`: the review panes paint it.** The current
  line is the reading line, the selection's preferred line (where a click, a
  change step or a restored position put the reader). When that line is
  unchanged, it wears `code_sample_row_fill(current_line)`, the preview's own
  rule, beneath the selection wash, in both layouts. A changed reading line
  keeps its change band, so each line still wears one band, as BPR-0202 asks.
  The role already reached two other review-window surfaces through the
  palette's `AlternateBase` (the Overview's side column and the Home landing
  card).
- **`diff_hunk_header_background` and `diff_hunk_header_text`: still painted
  by nothing, and the decision is the owner's.** The review window has no hunk
  header. The panes show whole documents, so a header row would break the
  equality of pane line N and document line N that every mapping relies on
  (`apply_group_boundaries`). A group's start is drawn as a rule in its own
  `diff_group_boundary_rule` role (BPR-0202). No other surface renders a hunk
  (`git grep -i hunk` over the Qt, application and core sources finds only
  that comment). Stopping the Colours card from offering the two roles changes
  RFC-BPR-0034 C5's accepted "all 47 roles", and retiring them is RFC-BPR-0006's
  decision. This card therefore does not make either change. See the ask below.

### What a reviewer sees (`55e9405f`)

Captures and pixel readings are in the
[BPR-0986 rendering note](../../verification/BPR-0986-review-pane-roles.md).
Tab-indented code now shows the preview's faint markers on every leading tab in
both panes. The reading line is always also selected in the panes, so its band
is always under the wash. It makes the reader's line a shade apart from the
rest of the washed selection: light (219, 233, 249) becomes (210, 227, 247), and
dark (20, 35, 57) becomes (27, 43, 66). The washed counterpart on the other
side, and every unselected line, are unchanged.

## Owner asks — parked on human-review

One question per criterion. No attended session was held; these are recorded
for the next one.

1. **AC1, the two hunk-header roles.** The review window has no hunk header,
   and adding one would break line identity. Should the Colours card (a) stop
   offering "Diff hunk header background" and "Diff hunk header text", with
   RFC-BPR-0034 C5 amended from 47 roles to 45 and the theme documents keeping
   both roles; or (b) keep offering them with a note that the review window
   does not show them yet; or (c) take their retirement to RFC-BPR-0006?
2. **The rendered change to the review panes.** Looking at
   `panes-light-after.png` and `panes-dark-after.png` beside their "before"
   captures: do you accept tab markers on every leading tab, and the reader's
   line a shade apart under the selection wash, as the review window's look?

## Evidence

macOS 26.6.2 arm64, `build/dev`, Qt 6.11.2, the change as committed in
`55e9405f`:

- The card's rows pass: `qt_syntax_pane_composition`,
  `qt_diff_workbench_scale_100/125/200`, `qt_type_preview`,
  `qt_code_sample_painter` and `qt_theme_preview_strip`. So do
  `qt_appearance_settings_panel`, `qt_appearance_role_display`,
  `qt_appearance_profile`, `qt_workbench_window_scale_100/125/200`,
  `qt_composed_github_scale_100`, `application_appearance_profile_consumers`,
  `application_native_appearance_probe_contract`, the three end-to-end
  appearance contracts (`application_appearance_theme_customization_contract`,
  `application_appearance_gesture_contract`,
  `application_appearance_store_fallback_contract`),
  `application_large_review_benchmark`, `application_large_review_resources`
  and `application_large_review_loaded_close`.
- New in `qt_diff_workbench_scale_*`:
  `test_the_reading_line_wears_the_current_line_band` requires the unchanged
  reading line to wear the role on its own side only, a changed reading line
  to keep its added band, and the one-pane reading row to wear it. The tabbed
  source course requires 12 markers per repaint of its two panes.
- **Negative controls.** With the tab-marker call off, the scale rows fail with
  "each pane marks its six leading tabs once per paint". With only the
  current-line bands off, they fail with "an unchanged reading line wears the
  current-line band, on its own side only". Restored, both pass.
- The first draft laid the band beneath every reading line, changed or not.
  That broke "the source of a move is painted as moved" in all three scale
  rows, because a line then wore two bands. The committed form emits it only
  where no change band is.
- clang-format 23.1.1 passes all three files, and their `HEAD` versions were
  clean. The project tidy script reports nothing on `diff_workbench.cpp` or its
  test.
