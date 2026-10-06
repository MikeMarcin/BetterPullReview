---
schema: "implement-rfc.card.v1"
id: "BPR-1296"
title: "macOS: the large-review byte-pressure course intermittently misses its 184 MiB memory limit"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"BPR-1290 optimized large-review rows, 2026-10-05","reservation":"docs/issues/_reservations/BPR-1296--BPR-1296--f65dd49cf8eb.json","related_cards":["BPR-0967","BPR-0893","BPR-1032","BPR-1264","BPR-1265"]}
acceptance: [{"criterion":"AC1 20 of 20 optimized macOS runs of the row pass at the candidate, with the distribution recorded","outcome":"pass","revision":"13e24274b","evidence":{"kind":"command","detail":"ctest -R ^application_large_review_cache_bytes$ -V, 20 serial runs in the tests-on Release tree at 13e24274b (macOS 26.6.2, M4 Pro, Qt 6.11.2): 20/20 pass; lifetime footprint growth 154.0/158.6/170.3 MiB (min/median/max) against 184; sampled heap peak growth 160.9. Before, at fb369625c: 19/20, 169.1/176.5/190.4, heap 173.3-174.0; the miss was 'sampled fixture footprint growth exceeded its platform budget' at 190.4 MiB."}},{"criterion":"AC2 the growth is attributed to its commit with before/after measurement","outcome":"pass","revision":"13e24274b","evidence":{"kind":"observation","detail":"Live heap at application_constructed is deterministic: 49.31 MiB at 5a26d6b1b, 58.90 at fb369625c. Bisection over 1,567 commits found three steps, all glyph icons pre-rendered at every mode/size/scale: acebc2089 (50.84->54.15), 3ac3d0f1f (54.47->55.28), 7f221a426 (55.71->58.96). Painting icons on request gives 45.63 MiB constructed, 63.23 loaded (from 76.37); the limit is unchanged."}},{"criterion":"AC3 a negative control shows the limit still fails when the growth returns","outcome":"pass","revision":"13e24274b","evidence":{"kind":"command","detail":"(1) Icon sources reverted to origin/main: constructed heap 58.90 MiB again and qt_review_cycle_view_1_0 fails 'strip: no pixmap is held before a view asks for one'. (2) Fix plus a touched 36 MiB block held from the byte course's start: the row fails 'sampled fixture footprint growth exceeded its platform budget' at cache_byte_selection_18 (185.9 MiB). (3) A real ::clone(0) in painted_glyph_icon.cpp still fails architecture_boundaries. Each control was restored byte for byte."}}]
---

# BPR-1296: macOS byte-pressure course misses its 184 MiB memory limit

## Observation

On macOS (M4 Pro, Qt 6.11.2, optimized tests-on tree)
`application_large_review_cache_bytes` intermittently fails on plain
`origin/main`: 195.1 MB and 198.7 MB measured against 192.9 MB (184 MiB),
recorded on [[BPR-1290]]. Windows passes it in 7 of 7 optimized runs. The
Windows Debug byte-course failure is a different cause, owned by [[BPR-1265]].

The limit was set by [[BPR-1032]] (10% above a 165.5-166.7 MiB peak) and
revalidated on Release at `5a26d6b1b` by [[BPR-0967]]: sampled heap growth
164.0 MiB, lifetime footprint growth 158.2 MiB.

## Outcome

Measure the row's distribution at `origin/main`, find what grew since the
budget was set, and repair a real regression. Re-baseline only if the
footprint is legitimately higher and stable, with evidence and the BPR-0967
budget rule; never raise the limit to absorb variance.

## Direct acceptance

- AC1: 20 of 20 optimized macOS runs of the row pass at the candidate, with
  the distribution recorded.
- AC2: the growth is attributed to its commit with before/after measurement.
- AC3: a negative control shows the limit still fails when the growth returns.

## Measurement

macOS Tahoe 26.6.2, M4 Pro, Qt 6.11.2, Apple clang, `cmake --preset dev`
with `CMAKE_BUILD_TYPE=Release` (tests on), the row run alone and serially.
Footprint is the largest lifetime-peak growth over `before_application`;
the limit is 184 MiB.

| Tree | Runs | Passed | Footprint growth MiB (min / median / max) | Sampled heap peak growth MiB |
|---|---:|---:|---|---:|
| `5a26d6b1b`, where BPR-0967 revalidated the limit | 3 | 3 | 157.1 / 158.2 / 182.1 | 164.0 |
| `fb369625c` (`origin/main`) | 20 | 19 | 169.1 / 176.5 / 190.4 | 173.3-174.0 |
| `13e24274b`, the fix | 20 | 20 | 154.0 / 158.6 / 170.3 | 160.9 |

The failing `origin/main` run ended `sampled fixture footprint growth
exceeded its platform budget` at `before_loaded_close`, 190.4 MiB
(199.6 MB), beside the 195.1 and 198.7 MB misses reported on [[BPR-1290]].
The footprint varies by up to 20 MiB between runs of one binary; the live
heap does not, so it located the growth.

## Diagnosis

The median moved 18 MiB. The course's live heap had risen by the same
amount at every sample from the start: 49.31 to 58.90 MiB once the
application was constructed, 64.90 to 76.37 once the review loaded, and
157.42 to 168.41 with the cache full. Per-selection growth (3.4 MiB, one
cache entry) was unchanged. Bisecting the constructed figure over the
1,567 commits found three steps, each a set of glyph icons rendered in
advance at every mode, size and scale (36 pixmaps for a strip or share
glyph):

- `acebc2089` ([[BPR-1157]]), the cycle review bar's strip icons: 50.84 to 54.15.
- `3ac3d0f1f` ([[BPR-1182]]), two more strip icons on that bar: 54.47 to 55.28.
- `7f221a426` ([[BPR-1285]]), the Share notes sheet's glyph icons: 55.71 to 58.96.

With only one pixmap per mode in the share icons, the constructed heap
fell from 58.90 to 55.95. The growth was real, not variance, so the limit
was not re-baselined.

## Fix

`painted_glyph_icon` (`src/adapters/qt/src/painted_glyph_icon.*`) is a
`QIconEngine` that paints the glyph at the size, scale and mode a view asks
for and holds no pixmap. `strip_icon`, `conversation_icon` and
`share_glyph_icon` use it. Constructed live heap is 45.63 MiB and the
loaded review 63.23, below `5a26d6b1b`. The review strip's own icons
predate the limit, so the fix frees 13.3 MiB, more than the 9.6 that had
accumulated. The 184 MiB limit is unchanged.

`architecture_boundaries` forbids a `clone(` call anywhere in `src/`. In
this file only, it now removes the exact `QIconEngine` override
declaration before the scan.

## Verification

- AC1: 20 of 20 at `13e24274b`. An earlier 20 at the same code before a
  comment and `QPoint` cleanup were also 20 of 20 (151.1 / 157.0 / 166.4).
- AC2: the bisection and before/after figures above.
- AC3, negative controls:
  - With the three icon sources reverted to `origin/main`, the constructed
    heap is 58.90 MiB again, and the new `paints_glyph_icons_on_request`
    check fails (`strip: no pixmap is held before a view asks for one`).
  - With the fix and a touched 36 MiB block held from the start of the
    byte course (headroom over 20 runs is 13.7-30.0 MiB), the row fails
    `sampled fixture footprint growth exceeded its platform budget` at
    `cache_byte_selection_18`, 185.9 MiB.
  - A real `::clone(0)` added to `painted_glyph_icon.cpp` still fails
    `architecture_boundaries`.
- Focused rows in the optimized tree, 133 selected by composition
  (`qt_workbench*`, `qt_composed*`, every `review_cycle*`, conversation,
  drawer and tour rows, all 13 large-review rows, first tab): 132 passed. The
  one failure was `architecture_boundaries` on the `clone()` override, fixed
  as above and passing. `qt_review_cycle_view_1_0/2_0` and the
  `paints_glyph_icons_on_request` icon check pass at 1x, 1.25x and 2x.
- clang-format clean on the touched files; clang-tidy clean on
  `painted_glyph_icon.cpp` and the three icon sources. In the test file, the
  only finding is the earlier `line_pixels` name.
