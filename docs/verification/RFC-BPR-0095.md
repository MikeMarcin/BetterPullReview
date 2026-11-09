# RFC-BPR-0095 verification

Mandate `diff-scroll-geometry-r1`, assigned to
`/root/diff_scroll_implementation`; one milestone `stationary-diff-geometry`.
RFC_BASE_SHA: `31559e616707ed106d0bd98f8d047df785c38fc0`.
Product and test candidate: `7e685a115ea5bdd8c6f956447470ba4788be1e19`.
Adoption: `1a28d5363`; claim: `e1fbb4503`. The supplied draft's four criteria
are unchanged; no independent, native-desktop or human acceptance is required.
The implement-rfc and frontend-design skills and desktop guidance were loaded.

## Changes and direct evidence

The unified heading uses the stationary viewport margin. Gutter numbers,
backgrounds, selection, number hits and open-thread indications apply the
within-row displacement once, with marks clipped below the fixed heading.
Find uses the retained displayed source area rather than the backing widget's
shifted origin or overscan. A compact count reserves the accepted result's
ordinal digit budget, so changing the current match cannot shrink or enlarge
the panel. Existing insets, query growth and narrow fitting are retained.
All changes stay in Qt presentation; no source identity, matching, host effect,
persistence, library or gesture-filter policy changed.

| Criterion | Direct course | Result |
|---|---|---|
| AC01 | Existing `qt_diff_workbench_scale_100`, `_125`, `_200`: native cursor rows versus raster glyph bounds for unchanged/removed/added rows, seven residual/crossing/reversal samples, fixed heading image, number clicks and open-thread bar | PASS at `7e685a115` |
| AC02 | Actual horizontal movement in unified and both split panes; production Find rectangles in window coordinates across vertical/horizontal input, explicit nonzero-residual placement refresh and next-match reveals; clipped-display oracle independent of production's retained rectangle | PASS at `7e685a115` |
| AC03 | Gutter raster after resize, 150% code zoom and number hiding/showing; composed Find after resize, zoom, number visibility, query height and presentation changes, including the existing 900/720 requested-width fitting courses | PASS at `7e685a115` |
| AC04 | `application_source_find`, `_scale_1_25`, `_scale_2_0` through the real composition, installed Git/cache and committed source-find fixture; full-window renders plus composed changed-row field comparisons; existing fractional/keyboard and source Find courses | PASS at `7e685a115` |

The functional courses run serially on macOS 26.6.2 arm64, Qt 6.12.0,
Debug, offscreen, at device scale 1.0, 1.25 and 2.0. The composed fixture
contains 1106 source lines and exercises 1000 retained-result navigations.
The unified raster fixture has replacement, deletion, insertion and unchanged
rows plus a 120-line tail. Native style/desktop, other operating systems,
Release performance, packaging and release qualification are not claimed.

Scroll observations compare geometry-rebuild, pane-measurement, semantic-index
and matching counters. Ordinary scroll performs no remeasurement or source
matching and retains the immutable index. Toggling number visibility may deliver
one deferred resize of the hidden split viewports on the next Qt turn; that
case permits only the recorded `viewport_or_divider` change with the
`pane_viewport_resize` trigger. It is a changed layout, not unchanged-axis
scroll work. Elapsed times are local functional-course observations, not a
performance guarantee. No dedicated Debug timing course was selected.

## Commands and artifacts

External command logs are retained under `/tmp/bpr-diff-scroll-execution`.
No transient logs, images, build outputs or credentials are tracked.

```sh
cmake --preset dev
cmake --build --preset dev --target test_better_pull_review_qt_diff_workbench_tests test_source_find_public_path --parallel 6
ctest --test-dir build/dev -R '^(architecture_boundaries|qt_diff_workbench_scale_(100|125|200)|application_source_find(_scale_(1_25|2_0))?)$' --output-on-failure
python3 _coding-standard/scripts/format.py --dry-run src/adapters/qt/src/diff_workbench_support.cpp src/adapters/qt/src/diff_workbench_support.hpp src/adapters/qt/src/diff_workbench_find_and_editing.cpp src/adapters/qt/src/source_find_bar.cpp src/adapters/qt/include/bpr/qt/source_find_bar.hpp tests/adapters_qt/diff_workbench_tests.cpp tests/end_to_end/source_find_public_path_tests.cpp
python3 _coding-standard/scripts/tidy.py --build-dir build/dev --paths src/adapters/qt/src/diff_workbench_support.cpp src/adapters/qt/src/diff_workbench_find_and_editing.cpp src/adapters/qt/src/source_find_bar.cpp --warnings-as-errors --jobs 3
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
git diff --check
```

Format uses clang-format 22.1.8; focused clang-tidy 22 passes all three changed
production translation units, with third-party warnings suppressed by the
existing tool policy. Documentation verification and all 67 documentation
controls pass before final bookkeeping; final closeout rechecks structure.
The final integration log and CTest `Testing/Temporary/LastTest.log` bind the
actual candidate. Full-window evidence is in `build/dev/source-find-renders`,
`source-find-renders-1_25` and `source-find-renders-2_0`; workbench rasters are
in `build/dev/tests/adapters_qt`. These are ignored local artifacts.

## Falsification and corrections during verification

Before the fix, the production source-Find course failed the stationary
window-rectangle assertion (`baseline-find.log`). A fresh negative control
against the final gutter oracle restored the original shifted-viewport heading
calculation; it failed precisely the fixed-heading assertion
(`negative-gutter.log`). The control restored the original candidate source
bytes in `finally`, followed by a successful rebuild. The source candidate
contains no negative-control switch.

Expanded narrow fitting exposed an additional AC02 defect: next-match
navigation changed the compact panel from 230 to 228 pixels with unchanged
layout because proportional count digits changed its minimum width.
Stable result-bound count reservation corrects this; the expanded composed
course directly observes that rectangle through reveals.

Early oracle failures were corrected rather than accepted: a sibling
`QWidget::mapTo` was replaced by mapping both widgets through the common
workbench; unchanged gutter/code backgrounds are intentionally different,
so field comparisons use changed rows. Bounded-work observations distinguish
the explicit margin transition's attributed resize from ordinary scrolling.
A premature test invocation overlapped a rebuild and was discarded; acceptance
uses the final serial integration run after the exact candidate build completed.
The source-Find harness now unwinds failures through the shared expectation
handler so its owned private temporary root can be cleaned up.

## Closeout

Accepted: all four criteria pass. The final serial integration passed all seven rows in 108.49 seconds: architecture 19.56s; workbench 9.71/9.92/11.66s; composed Find 19.69/18.60/19.31s at 100/125/200%. Full-window renders at all scales were generated; the 100% unified, 125% split and 200% unified renders were inspected. No remote mutation, owner desktop operation, push,
release or attended session was performed. The
[durable checkpoint](../issues/_checkpoints/BPR-1947/handoff.md) retains command
state, selection and the explicit selected-plan binding. Target-only discovery
refused an unrelated legacy plan's unknown `amendments` field; the installed
helper's explicit selected-plan API bound this milestone without modifying that
plan. The logical completion check remains separate from engineering evidence.

Mutable execution sections were normalized to the helper's recognized headings before final binding renewal. The preserved [initial binding](../issues/_checkpoints/BPR-1947/binding-initial.json), complete criteria and plan remain available; no acceptance requirement changed.
