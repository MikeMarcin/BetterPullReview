---
schema: "implement-rfc.card.v1"
id: "BPR-1533"
title: "A text selection in the comparison panes reads as clearly as in Line details, on every line kind"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner report 2026-10-15","owner_note":"At least on dark theme I'm finding it quite hard to see a selected block (eg a word double clicked) in the middle of a change diff on the main pane. Partially because of the line highlight color, but I don't think that's the only cause. Actually it's not so easy to see even on unchanged lines. The single line details pane shows it much more clearly for some reason.","governing_rfc":"RFC-BPR-0006","related_rfcs":["RFC-BPR-0028","RFC-BPR-0034"],"related_cards":["BPR-1212","BPR-1156","BPR-1334","BPR-1503","BPR-1534"],"reservation":"docs/issues/_reservations/BPR-1533--BPR-1533--5c45415baeca.json","requested_skill":"frontend-design"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c84681b0a09ed2935f0f098cbdeb777b1ae95a14","evidence":{"kind":"command","detail":"qt_diff_workbench_scale_{100,125,200} line-kinds course: Light, Dark, HC Light, HC Dark x unchanged, removed, added, changed, focused and unfocused: fill is code_selection_background, 45-102 sRGB units from the ground (Dark 1.46-1.79:1 on the selected line, 2.38:1 off it); baseline paint fails (17-29 units, 0 inside a change)."}},{"criterion":"AC2","outcome":"pass","revision":"c84681b0a09ed2935f0f098cbdeb777b1ae95a14","evidence":{"kind":"command","detail":"qt_code_sample_painter: append_text_selection_ranges keeps a readable role exactly, moves a faint one, every run at the variant's text floor; BPR-1212 syntax course expects the selection ink, not HighlightedText, and fails against the wash."}},{"criterion":"AC3","outcome":"pass","revision":"c84681b0a09ed2935f0f098cbdeb777b1ae95a14","evidence":{"kind":"command","detail":"28/28 focused ctest rows incl. application_source_find x3 (current match takes no selection), application_line_details_public_path, qt_workbench_window_scale_{100,125,200}, review-tour, overview, appearance; clang-format 22, clang-tidy, DOCUMENTATION_OK."}},{"criterion":"AC4","outcome":"pending","revision":"c84681b0a09ed2935f0f098cbdeb777b1ae95a14","evidence":{"kind":"freeform","detail":"Owner Accept/Reject pending in fresh session 7c8004e6428577726d1ce8825ee1dff5, candidate BPR-1533-selection-v001, fingerprint cfc14897d92f544bd5d618d95f1c6ce05f3eedf9543295dec3b4e48ae7a7b36c, cursor 8; frozen root bpr1533-selection-v001.u0prev45."}}]
---

# BPR-1533: A text selection in the comparison panes reads as clearly as in Line details, on every line kind

The owner reported on 2026-10-15: "At least on dark theme I'm finding it
quite hard to see a selected block (eg a word double clicked) in the middle of
a change diff on the main pane. Partially because of the line highlight color,
but I don't think that's the only cause. Actually it's not so easy to see even
on unchanged lines. The single line details pane shows it much more clearly
for some reason."

Governing design: [RFC-BPR-0006](../../rfcs/RFC-BPR-0006-syntax-highlighting-themes-appearance.md)
(the theme's `code_selection_background` role and the code canvas's text
floor), under the review workbench of
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md). The
panes' selection treatment is recorded in the
[workbench specification](../../ux/workbench-specification.md). The companion
report, that Line details loses the syntax colours under a selection, is
[[BPR-1534]]; the two must agree on one treatment.

## Acceptance

- AC1: In both comparison panes (two-pane and one-pane), a text selection
  (a double-clicked word, a drag, Shift+Up/Down) is drawn over everything the
  line already wears: the unchanged canvas, the added and removed fills, the
  intraline emphasis and the selected line's band, in the same solid theme
  selection colour Line details uses. Measured offscreen in Light, Dark, High
  Contrast Light and High Contrast Dark, focused and unfocused, on unchanged,
  removed, added and changed lines, the fill is that colour and stands at
  least 40 sRGB units from the ground beside it (the wash it replaces measured
  0 to 37); off the selected line it measures as Line details does.
- AC2: Selected glyphs keep their syntax role colours, moved only as far as
  they must to read at the variant's text floor (4.5:1, 7:1 for high
  contrast) on the selection; no hard-coded colour, no new setting; the
  colour comes from the theme's `code_selection_background`.
- AC3: Copy, Find's current match (no selection stacked on it), the line
  band, gutter and keyboard and mouse selection behaviour are unchanged.
- AC4 (human): The owner accepts the exact frozen candidate, shown with
  offscreen Dark and Light before and after renders of a double-clicked word
  on unchanged, added, removed and current lines, and Line details beside it.

Amendment (implementer, 2026-10-15, before any evidence was recorded): AC1
first asked for a separation "at least as much as Line details' selection does
today" on every line. On the selected line that is not reachable without
changing the selected line's wash: the word sits inside the line's own wash,
which Line details does not paint (Dark 1.46–1.79:1 against Line details'
1.67–2.38:1). AC1 now states what is measured; whether that reads clearly
enough is the owner's AC4.

## Implementation

Load frontend-design:frontend-design before choosing the treatment. Qt
presents: the change is the Qt adapter's paint of the panes' text selection,
shared with Line details; no core, host, provider or storage change unless
the contrast contract needs a new pair. Never drive or capture the owner's
desktop; renders are offscreen. Visual judgement goes through a fresh short
attended-review session and a frozen build.

## Checkpoint — candidate v001, 2026-10-15

Product candidate `c84681b0a` on branch `bpr-1533-selection` (worktree
`/Users/jaeger/.codex/worktrees/bpr1533-selection/BetterPullReview`), on
`main` at `1bd314121`; the attended render harness is `c16ce09b1` (tests
only on top). Commits: `1cf1db2f1` the shared helper (no behaviour change),
`a0538e749` this card, `c84681b0a` [[BPR-1534]], `c16ce09b1` a render look.
This card's commit and BPR-1534's each cherry-pick onto the helper alone. Not
merged to `main` until the owner accepts AC4. Paths, commands and identities
are in [the checkpoint](../_checkpoints/BPR-1533/handoff.json).

Design (frontend-design loaded first): the text selection is the one strong
layer, solid in the theme's `code_selection_background` (the pane's
`Highlight`) and painted last, over the row band, the selected line's wash and
the intraline emphasis, as Line details, every text box and Review Cycles'
code view ([[BPR-1156]] v006, owner-accepted) draw one. Each selected glyph
keeps its role colour through `core::readable_selection_ink`. Rejected: Qt's
one-colour `HighlightedText` (the owner rejected it in [[BPR-1212]] v001) and
the translucent wash (the defect). No setting, no hard-coded colour, no core
change.

Root causes, measured offscreen in the four built-in variants with the same
harness before and after:

1. The panes drew the text selection as `Highlight` at
   `core::selection_wash_alpha` (72/255), the same colour and opacity as the
   selected line's wash. A double click selects its line first, so the word
   was that wash twice over its own line's wash: 1.10–1.16:1 and 22–29 sRGB
   units in Dark, 1.14–1.16:1 in Light, 1.00–1.05:1 in high contrast.
2. The wash was inserted before the intraline emphasis, whose fill is opaque,
   so inside a change the selection was not drawn at all: 1.00:1, 0 units, in
   every variant.
3. Line details used Qt's own opaque selection, which is why it read clearly
   (Dark 2.38:1 on an unchanged row). Focus is not a cause: the profile
   gives `Inactive` the same `Highlight` as `Active`.

| Dark, selection against the ground beside it | Before | After | Line details |
|---|---|---|---|
| Unchanged, selected line | 1.16:1, 24 | 1.79:1, 84 | 2.38:1, 136 |
| Removed, selected line | 1.12:1, 25 | 1.64:1, 87 | 1.86:1, 122 |
| Added, selected line | 1.10:1, 23 | 1.46:1, 80 | 1.67:1, 111 |
| Part of a changed run, selected line | 1.00:1, 0 | 1.39:1, 45 | — |
| Unchanged, not the selected line | 1.19:1, 39 | 2.38:1, 136 | — |

Light after: 1.73, 1.65, 1.76, 1.02 (48 units, hue only: the selected row's
deeper emphasis, [[BPR-0323]], is as light as the light selection) and
2.26:1. High contrast after: 45–102 units in every case. The fill is the
theme's selection colour, focused and unfocused, in all sixteen cases.

| Acceptance | Evidence at `c84681b0a` | Result |
|---|---|---|
| AC1 | `qt_diff_workbench_scale_{100,125,200}`: new line-kinds course, four variants by unchanged, removed, added and changed lines, focused and unfocused: fill is the theme selection, ≥ 40 sRGB units from the ground (45–102 measured). Negative control: the baseline paint fails ("the selection is the theme's solid selection colour"; 17–29 units, 0 inside a change) | Pass |
| AC2 | `qt_code_sample_painter`: the helper keeps a readable role exactly, moves a faint one, every run at the text floor; BPR-1212's syntax course expects the selection ink, not `HighlightedText` (fails against the wash) | Pass |
| AC3 | `application_source_find{,_scale_1_25,_scale_2_0}` (current match takes no selection on top), `application_line_details_public_path`, `qt_workbench_window_scale_{100,125,200}`, review-tour, overview and appearance courses: 28 of 28 pass; Copy's selection unchanged in the course | Pass |
| AC4 human | Fresh session (below), Dark and Light before/after sheets and the frozen build | Pending |

Gates: clang-format 22.1.8 clean on the seven touched C++ files; clang-tidy
passed on the five touched translation units; documentation gate
`DOCUMENTATION_OK` and its 67 tests pass. Builds and renders were offscreen;
no owner desktop was driven or captured.

Attended: fresh session `7c8004e6428577726d1ce8825ee1dff5` (state `/Users/jaeger/.codex/attended-review/bpr1533-v001-aCNnPi`), cursor 8, one
Accept/Reject question per card: `BPR-1533-selection-v001` (fingerprint
`cfc14897…`) and `BPR-1534-line-details-syntax-v001` (`d9dda508…`), both
ready and protected, on the frozen root
`/Users/jaeger/.codex/attended-builds/bpr1533-selection-v001.u0prev45`
(`c84681b0a`, fixture `line-details-v1`, opens `src/report/query.cpp`; its own
`--check` reached `ATTENDED_FILE_SHOWN:src/report/query.cpp`; executable
SHA-256 `f429c39b5ba91f82111a9ba9d3dab1ae3a9aa29fc31477f38ca6f21013097c11`).
The review server (pid 70713) is handed to the coordinator.

Next: on hand-back, an Accept merges this card's commit (with the shared
helper) to `main` and finishes the card; a Reject's note becomes v002.
