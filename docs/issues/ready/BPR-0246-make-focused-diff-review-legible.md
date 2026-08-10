---
id: BPR-0246
title: Make focused diff review legible and regenerate UIforETW evidence
status: Ready
rfc: RFC-BPR-0006
depends_on: [BPR-0227]
---

# Make focused diff review legible and regenerate UIforETW evidence

The first attended UIforETW batch was rejected. At 1280 × 1000 the Files and
Conversations drawers plus the oversized pull-request, epoch, and status chrome
leave too little space for the two code panes and gutter. The long `.rc` lines
are clipped before the known changed coordinates, and the selected change is an
insertion boundary. Its ribbon therefore appears to point from nothing to
nothing instead of explaining a visible intraline replacement.

The product owner's exact v001 verdict is preserved in
`docs/verification/reviews/uiforetw-intraline-acceptance-v001/human-verdict.json`
and its digest-bound validation record is adjacent. Additional review feedback
requires the full-window focused-review state—not merely a crop—to devote much
more of the window to the two code panes and gutter, with the Files drawer
collapsed as a real reviewer would collapse it.

## Required final state

- Preserve the shipped Files-drawer selection route, exact recorded commits and
  blobs, the Resource `136` → `137` and RC `464,28` → `464,24` / `464,42` →
  `464,36` pairs, semantic roles, and the real selection-wire cut failure.
- Enter focused review through shipped UI actions after file selection: collapse
  the Files and Conversations drawers while retaining visible, accessible
  controls that reopen them. Do not crop the drawers out or hide them through a
  tests-only product bypass.
- At the retained 1280 × 1000 logical/pixel evidence viewport, make the combined
  code panes and gutter the dominant full-window region: at least 75% of client
  width and 60% of client height. Compact the upper pull-request/epoch/status
  presentation enough to meet that bound without discarding its information.
- Navigate to each named replacement rather than an insertion boundary. Reveal
  its exact removed and added intraline rectangles horizontally; each rectangle
  must be nonempty, wholly within its pane viewport, and contain nonzero rendered
  pixels of its exact semantic role.
- For each visible replacement ribbon, assert both pane-edge endpoints and
  vertical overlap with the visible target rows. Retain the existing exact
  connector geometry tests; do not claim a connector implementation defect
  unless this stronger public-path evidence exposes one.
- Persist the full-window, pane, gutter, crop, target-selection, connector, and
  drawer rectangles; horizontal offsets; role-pixel counts; selected change
  identity; and layout ratios in the capture facts. The registered contract must
  fail if any required fact is missing or false.
- Generate a new immutable four-profile attended batch under
  `uiforetw-intraline-acceptance-v002`. Preserve rejected v001. BPR-0233,
  BPR-0214, BPR-0210, and RFC-BPR-0006 remain open unless v002 is accepted.

## Verification

```sh
ctest --preset dev -R '^application_uiforetw_selected_diff_contract$' --output-on-failure
ctest --preset dev -R '^(qt_diff_link_map_scale_100|qt_diff_workbench_scale_100)$' --output-on-failure
python3 scripts/verify_documentation.py --root .
```
