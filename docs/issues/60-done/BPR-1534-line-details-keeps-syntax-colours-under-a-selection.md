---
schema: "implement-rfc.card.v1"
id: "BPR-1534"
title: "Line details keeps the syntax colours under a text selection"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner report 2026-10-15","owner_note":"Although I see the line details lists syntax highlights on selecting which is a separate bug","governing_rfc":"RFC-BPR-0006","related_rfcs":["RFC-BPR-0028"],"related_cards":["BPR-1334","BPR-1212","BPR-1156","BPR-1533"],"reservation":"docs/issues/_reservations/BPR-1534--BPR-1534--78ec543a573a.json","requested_skill":"frontend-design"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"5bde3d8dd","evidence":{"kind":"command","detail":"On main at 5bde3d8dd: qt_diff_workbench_scale_{100,125,200} Line details course, four variants: selected int in its selection ink (4.51/5.65/8.84/7.03:1) not HighlightedText; Qt's paint fails in every variant (negative control at the candidate)."}},{"criterion":"AC2","outcome":"pass","revision":"5bde3d8dd","evidence":{"kind":"command","detail":"Same course: fill is code_selection_background via the shared append_text_selection_ranges (b38953f19); 0 pixels differ outside the selection; Copy unchanged; application_line_details_public_path and qt_code_sample_painter pass; DOCUMENTATION_OK."}},{"criterion":"AC3","outcome":"pass","revision":"5bde3d8dd","evidence":{"kind":"observation","detail":"Owner accepted BPR-1534-line-details-syntax-v001 in session 7c8004e6428577726d1ce8825ee1dff5 (event 10, fingerprint d9dda508...) on the frozen c84681b0a build; product sources identical to 5bde3d8dd."}}]
---

# BPR-1534: Line details keeps the syntax colours under a text selection

The owner reported on 2026-10-15, beside [[BPR-1533]]: "Although I see the
line details lists syntax highlights on selecting which is a separate bug".
The reading is "loses": selecting text in Line details ([[BPR-1334]]) is
expected to repaint the selected glyphs in one colour. What actually happens
is measured offscreen and recorded below before any change.

Governing design: [RFC-BPR-0006](../../rfcs/RFC-BPR-0006-syntax-highlighting-themes-appearance.md),
as for [[BPR-1533]]. The comparison panes keep the syntax colours under a
selection since [[BPR-1212]] v002 (the owner, on v001: "are we meant to lose
all syntax highlighting when we select something?"), and Review Cycles' code
view since [[BPR-1156]] v006.

## Acceptance

- AC1: In Line details a text selection keeps each selected glyph's syntax
  role colour, moved only as far as it must to read at the variant's text
  floor on the selection, in all four built-in variants; plain text and the
  intraline emphasis read the same way.
- AC2: The selection is the same treatment the comparison panes use
  ([[BPR-1533]]): one shared helper, the theme's `code_selection_background`.
  Copy, the gutter, the bands, the placeholder and the horizontal scroll are
  unchanged.
- AC3 (human): The owner accepts the exact frozen candidate's Line details
  with a selection, shown before and after in offscreen Dark and Light renders.

## Implementation

Separate commit from [[BPR-1533]], so either can merge alone. Qt presents;
no core, host or storage change. Renders offscreen only.

## Checkpoint — candidate v001, 2026-10-15

What actually happened, measured offscreen before any change: the reading
"loses" is right. Line details was a plain `QPlainTextEdit` whose selection
Qt paints in `HighlightedText`, repainting every selected glyph in one
colour. A selected `int` (type role) became Light `#1f2328` for `#6e3ec8`,
Dark `#dcdcdc` for `#ffd700`, High Contrast Light `#000000` for `#5a009e`
and High Contrast Dark `#ffffff` for `#dbaaff`. The selection's fill was
already the solid theme selection; only the glyph colours were lost.

Candidate: commit `c84681b0a` on branch `bpr-1533-selection`, beside
[[BPR-1533]]'s `a0538e749` on the shared helper `1cf1db2f1`; it cherry-picks
onto the helper alone and passes there, so either card can merge without the
other. `line_details_pane::paintEvent` paints as Qt does, except that the
cursor selection goes through `append_text_selection_ranges`, the comparison
panes' treatment: the same solid fill, each glyph in its role colour moved only
as far as `core::readable_selection_ink` needs. With nothing shown (the
placeholder) or no selection, Qt paints. Not merged to `main` until the owner
accepts AC3. Paths, commands and identities are in
[the checkpoint](../_checkpoints/BPR-1534/handoff.json).

| Acceptance | Evidence at `c84681b0a` | Result |
|---|---|---|
| AC1 | `qt_diff_workbench_scale_{100,125,200}`: new Line details course in four variants: the selected `int` is drawn in its selection ink (4.51, 5.65, 8.84, 7.03:1, each at the variant's floor) and not `HighlightedText`. Negative control: Qt's paint fails ("the selected type keeps its colour…") in every variant | Pass |
| AC2 | The same course: the fill is the theme selection, the same helper as the panes ([[BPR-1533]]); 0 pixels differ outside the selection against the unselected paint with the caret in place; Copy's selection unchanged; `application_line_details_public_path` passes | Pass |
| AC3 human | Fresh session (below), four-variant before/after sheet and the frozen build | Pending |

Gates as [[BPR-1533]]: clang-format 22.1.8, clang-tidy, documentation gate
and its tests pass. Renders offscreen only.

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

## Owner verdict and merge — 2026-10-15

The owner accepted `BPR-1534-line-details-syntax-v001` (session
`7c8004e6428577726d1ce8825ee1dff5`, decision event 10, fingerprint
`d9dda508…`, handed back at event 11) on the frozen `c84681b0a` build. The
shared helper and this card's commits were cherry-picked onto `main` at
`0b37d5438` as `b38953f19` (helper), `5bde3d8dd` (this card) and `1d2564527`
(the render look, tests only), without [[BPR-1533]]'s commit, which the owner
rejected for a v002. On that tree `qt_diff_workbench_scale_{100,125,200}`,
`qt_code_sample_painter` and `application_line_details_public_path` pass (5 of
5), the Line details course reports the same inks (4.51, 5.65, 8.84, 7.03:1),
and the documentation gate reports `DOCUMENTATION_OK`.

