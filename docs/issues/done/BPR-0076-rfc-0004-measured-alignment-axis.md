---
id: BPR-0076
title: Implement Qt measured alignment axis
status: Done
rfc: RFC-BPR-0004
depends_on: [BPR-0074]
---

# Implement Qt measured alignment axis

Measure live left/right line-boundary document coordinates and build one
cached piecewise alignment axis. Each positive segment extent is the maximum
of measured left/right extents; empty sides project to a constant boundary and
unequal replacements converge at one comparison endpoint. Add indexed lookup,
legal end caps, clamping, and explicit invalidation.

Measured pixels remain Qt-local. Rebuild only for accepted diff/file/epoch,
font, zoom, line spacing, tabs, DPI/screen, viewport/divider, wrap/fold, or
document-layout changes—not ordinary vertical scroll.

## Required direct evidence

Prove continuity, zero-extent safety, insertion/deletion pinning, unequal
ratios, different viewport heights, start/end reachability, DPI/zoom/resize
invalidation, semantic-ID retention, and no Qt type or pixel state in core or
durable host state.

## Implementation record

- `bpr::qt::measure_plain_text_pane` measures the accepted logical-line count
  from a live natural-layout `QPlainTextEdit` in one forward block pass. Qt's
  trailing visual block supplies at most a final boundary and never becomes a
  source line.
- `bpr::qt::measured_alignment_axis` retains every canonical segment ID,
  derives each comparison extent as the maximum of the measured side extents,
  excludes only zero-length presentation intervals from its binary-search
  index, clamps to its exact start/end domain, and projects both panes without
  division by zero.
- `bpr::qt::measured_alignment_axis_cache` clears stale geometry, coalesces
  the complete explicit invalidation vocabulary, and advances one geometry
  generation per successful settled rebuild. Ordinary position projection
  neither invalidates nor rebuilds the axis.
- The focused Qt fixture directly covers right-insertion and left-deletion
  pinning, 4:7 and 7:4 proportional replacements, continuity, empty and hidden
  zero-height segments, legal endpoint clamping, invalid measurement rejection,
  semantic-ID retention, coalesced invalidation, independent viewport heights,
  and live font/tab/resize/DPI measurement at 1.0x, 1.25x, and 2.0x.

## Verification record

- `cmake --preset dev`
- `cmake --build --preset dev --target test_better_pull_review_qt_diff_axis_tests`
- `ctest --test-dir build/dev --output-on-failure -R '^qt_diff_axis_scale_(100|125|200)$'` — 3/3 pass
- `python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --paths src/adapters/qt/src/measured_alignment_axis.cpp tests/adapters_qt/measured_alignment_axis_tests.cpp --warnings-as-errors` — pass
- Rendered live-pane artifacts are generated under
  `build/dev/tests/adapters_qt/diff-axis-pane-{100,125,200}.png`; the 1.25x
  and 2.0x rasters were directly inspected. They are local evidence, not
  committed product assets or a BPR-0078 ribbon claim.
- A source scan finds no `qreal`, `QPlainTextEdit`, measured-axis, or pane-
  measurement state in `src/core/` or `src/application/`.

## Documentation impact

- `docs/verification/RFC-BPR-0004.md`: updated with the implemented Qt-local
  seam, direct evidence, and deliberately open controller/ribbon/composition
  claims.
- Product authority, GitHub/GHES, authentication, persistence, mutation,
  package, and launch behavior: `no-impact`; this slice adds derived Qt-local
  geometry only and performs no remote effect.
- The final integrated UX, agent-documentation, and full immutable-base ledger
  remain assigned to BPR-0081. Target-native Windows execution remains open
  without blocking this available-platform slice.

## Simplification review

The focused code-simplifier pass replaced repeated per-boundary rescans with
one forward Qt block walk, retained semantic kind/group facts beside each
measured ID for later projections, and removed geometry-based kind inference
from the tests. No further behavior-preserving reduction was justified; the
separate measurement, immutable axis, and invalidatable cache concepts encode
distinct ownership needed by BPR-0077 and BPR-0078.
