---
id: BPR-0246
title: Make focused diff review legible and regenerate UIforETW evidence
status: Done
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
collapsed as a real reviewer would collapse it. The clarification is broader
than that drawer: every surface other than the two code panes and their gutter
is on a strict space budget in the review state, especially the upper
pull-request information.

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
  width and 85% of client height. Compact the upper pull-request/epoch/status
  presentation enough to meet that bound without discarding its information.
  The focused state must not reserve large persistent regions for any non-diff
  surface.
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

## Implemented state

Focused review now uses only shipped controls. The public course selects the
recorded `.rc` file through the Files drawer, navigates to the exact replacement
group, then collapses both drawers and toggles Review details off. That retained
toolbar action hides and restores the secondary review header, epoch/history
controls, and selected-change detail without clearing their state. The native
comparison splitter—not a hand-cropped substitute—is captured after both named
intraline spans are revealed.

The v2 capture facts bind the full window, comparison, pane, gutter, hidden
surfaces, horizontal offsets, exact span rectangles and role-pixel counts,
selected segment/group, and connector endpoints/row overlap. The registered
contract rejects a missing or internally inconsistent fact and retains the real
Qt selection-wire negative. It navigates to both named replacement groups and
records exact-role pixels while each is selected. The selected-row wash is
painted below the opaque intraline role, and the pixel counter matches that role
colour rather than merely counting non-base pixels. All four explicit
appearance profiles produce the same focused geometry. The immutable attended
v002 batch is generated from committed implementation `d2ff0c1` at
`docs/verification/reviews/uiforetw-intraline-acceptance-v002/offline-package/index.html`.
It is digest-bound as
`bpr:/attended-review/betterpullreview/198d1045d20ff4752b7dc747`. Its verdict is
not inferred here; BPR-0233 remains open for that attended decision.

Independent critical review first found incomplete pixel/fact causality and a
false-green layout-ratio check. After two correction rounds it returned CLEAR
with no blocking or high-risk findings. The committed four-profile course then
passed in 70.85 seconds, with the comparison occupying 1262 × 890 pixels of the
retained 1280 × 1000 full window.
