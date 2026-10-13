# RFC-BPR-0073 verification

The `smooth-comparison-panning` milestone implements the owner’s smooth-motion,
minor-drift suppression and discrete-keyboard amendments. Its immutable RFC base
is `268b87049592ca4643e5303ab77114e116e7b5f3`; its plan and import provenance are in
[the adopted plan](../rfcs/plans/smooth-diff-touchpad-panning/MILESTONES.md) and
[live import](../rfcs/assets/smooth-diff-touchpad-panning.LIVE_IMPORT.md).

The owner accepted AC07 touchpad behavior on v002 and rejected the keyboard
reveals on v001, v002 and v003. The latest explicit correction requires the
selected row bottom flush with the actual displayed bottom after Down scrolls.
This replaces top-source-boundary rounding for reveals; page and endpoint
behavior remain unchanged. The v004 correction and exact candidate are recorded
in the [continuation](../issues/_checkpoints/smooth-diff-touchpad-panning/README.md).
The owner subsequently observed v004 alignment fixed but rejected its horizontal
scrollbar wake. The separate native scrollbar judgment remains pending. Fixture success does not
establish a human verdict or physical touchpad feel.

## Implementation and design review

The required `frontend-design:frontend-design` skill was read from the trusted
owner-supplied source at
`/Users/jaeger/.claude/plugins/marketplaces/claude-plugins-official/plugins/frontend-design/skills/frontend-design/SKILL.md`.
The interaction preserves the established native surface and gives accepted
motion a direct visible result, without easing or a new visual design.

The native `QPlainTextEdit` viewport moves by the rounded within-line residual,
with a fixed one-line overscan and a mask preserving its original content area.
The retained origin keeps its higher precision. Native painting, selection,
hit testing and child anchors consequently share the same physical placement;
gutters and ribbon endpoints use that applied placement. No private Qt API,
second text layout or painter-only translation is involved. An explicit scroll
observation publishes a frame even when the line scrollbar value is unchanged.

The Qt-local gesture discriminator uses delivered deltas before horizontal
clamping. A component at least twice the other establishes its predominant
axis and suppresses perpendicular noise. A signed 12-pixel perpendicular
intent threshold releases or changes that preference when recent input becomes
comparable or points along the other axis. Alternating wobble cancels that
evidence; renewed predominant motion clears it. Momentum retains the established
preference. The evidence threshold never becomes scroll distance: discarded
motion is not applied later. Begin/end,
device and comparison/presentation changes reset context; an unphased event
after 250 ms also resets it. Forwarding preserves device, phase, source,
inversion and timestamp. These constants are implementation tuning, informed by
the diagnostic below; they do not claim to reproduce Sublime's algorithm and
were accepted by the owner under AC07 on v002.

Keyboard cursor/selection movement retains the fraction while the target is in
view. A clipped target meets the actual displayed bottom (or top for Up) within
one device pixel. The region is captured before selection projection and
intersects the native viewport mask: Qt's `visibleRegion()` alone includes
overscan behind the horizontal scrollbar. No extra source-boundary rounding is
applied. Subsequent reveals scroll one source row per arrow, while the split
comparison authority still projects the other pane through unequal replacements.
Unified scrolling retains the required native row and within-row residual.
Page destinations remain line-aligned and Home/End retain exact endpoints.

The refinement pass kept the shared origin, cached measurements and native
geometry together. Two optional assignments and six local test identifiers were
also made consistent with the static gate; those changes preserve behavior.

## Criterion evidence

| Criterion | Result | Direct evidence |
|---|---|---|
| AC01 | Pass | `test_fractional_native_viewport` and `test_fractional_unified_and_keyboard_viewports` send sub-line pixel motion to real source viewports. Native cursor rectangles and captured viewport pixels move before the line scrollbar advances, hold the residual and retrace accepted reversal. |
| AC02 | Pass | Controller course strengthens repeated 20-pixel horizontal deltas to ±7 vertical noise, starts at the horizontal edge, reverses direction, changes receiving pane and includes momentum/end. Shared vertical origin stays fixed. Separate tests cover isolated and alternating perpendicular wobble. The unified composed course observes actual source-row placement during horizontal motion. |
| AC03 | Pass | Small vertical-only input, sustained comparable-axis diagonal input and within-gesture reorientation are tested, including a turn after 1,000 horizontal inputs so old travel cannot prevent release. Explicit device, phase, idle-unphased, comparison and presentation resets are covered. A viewport observer verifies forwarded pointing device, phases and timestamp. |
| AC04 | Pass | Composed native geometry checks compare ribbon endpoints with source boundaries at fractional origins, verify actual partially clipped row identity and selection, and observe conversation-marker movement with the source. The same course runs at 100%, 125% and 200%, then repeats fractional ribbon checks after 150% code zoom. Existing gutter/mark and ribbon negative-control checks remain active. |
| AC05 | Pass | Existing controller/workbench/link-map checks retain insertion pinning, unequal replacements, Home/End, scroll-past-end, pane handoff, angle/Shift-wheel, modified zoom, reveals and reading-anchor restoration. Range sweeps now observe real rendered origins rather than aliased integer line-bar values; the exact endpoint remains asserted. One-pixel reversal from End is explicit. |
| AC06 | Pass | The composed 2,000-line course sends 1,000 accepted pixel inputs and observes 1,000 pixels of actual native source travel within one pixel. Counters require no semantic query/index adoption, geometry rebuild or pane scan; 1,000 frames coalesce to one repaint delivery. Link work stays within near-visible candidates and retained buffers do not grow. Existing publication, resize and input timing bounds remain active. |
| AC07 | Owner accepted v002 | Session `c595e5c955c2cd527f9f4a351d3ab175`, candidate `BPR-1413-AC07-v002`, decision `0a0ba98a-3e76-4cb8-b404-fb0196dbd48c`. The adaptive axis implementation is unchanged in v003. |
| AC08 | Agent pass; owner v005 scrollbar judgment pending | The production font and a nonintegral-row-height viewport exercise ninety Down and ninety Up events with repeat and alternating Shift. Every clipped target must meet the actual displayed edge within one device pixel; consecutive Down reveals move exactly one row. An independent check requires the source bottom to meet the native horizontal scrollbar top, excluding overscan. Fully visible targets retain their rectangles. Both split panes, a 4:7 replacement and unified view are covered, with shared ribbon geometry and subsequent one-pixel touchpad continuation. |

## Commands and actual results

On macOS with Qt 6.11.2, all rendered checks use the offscreen platform. The
official CTest registrations run inside the repository’s network isolation.

```sh
cmake --build --preset dev --target test_better_pull_review_qt_diff_workbench_tests test_better_pull_review_qt_comparison_scroll_controller_tests test_better_pull_review_qt_diff_link_map_tests -j 4
ctest --preset dev -R '^(qt_diff_workbench_scale_(100|125|200)|qt_comparison_scroll_controller_scale_(100|125|200)|qt_diff_link_map_scale_(100|125|200)|architecture_boundaries)$' --output-on-failure
```

The revised official build exited 0. All 10 integration tests passed, including
architecture, in 136.07 seconds. These run the stricter current courses at all
three scales. Historical M1 results remain in `m1-verification.json`; the current
revision and results are recorded in `m2-verification.json` in the continuation.

```sh
python3 _coding-standard/scripts/format.py --dry-run src/adapters/qt/include/bpr/qt/pixel_scroll_gesture.hpp src/adapters/qt/include/bpr/qt/diff_workbench.hpp src/adapters/qt/src/diff_workbench.cpp tests/adapters_qt/comparison_scroll_controller_tests.cpp tests/adapters_qt/diff_workbench_tests.cpp
python3 _coding-standard/scripts/tidy.py --build-dir build/dev --paths src/adapters/qt/src/diff_workbench.cpp src/adapters/qt/src/comparison_scroll_controller.cpp tests/adapters_qt/diff_workbench_tests.cpp tests/adapters_qt/comparison_scroll_controller_tests.cpp --warnings-as-errors
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
git diff --check
```

Formatting uses clang-format 22.1.8. All four scoped translation units pass tidy.
Documentation reports `DOCUMENTATION_OK`; all 67
documentation verifier tests pass. Whitespace checks pass.

## Post-v002 keyboard correction

Candidate `a9af17c712f2fc3e7dc4c4718689ff482af8fed7` changes only the two
keyboard reveal boundary captures and their rendered regression course. AC07's
accepted adaptive axis policy and fractional native placement are unchanged.
The owner screenshot was inspected privately, without desktop capture. A
reproduction with the production font found target line 150 at y367–381,
fully inside the displayed y22–386 area. Down nevertheless moved it upward by
15 pixels; the fixed mask measured only 343 pixels while the displayed region
measured 365. The new test fails on unchanged v002 at “Down leaves a fully
visible target at its existing position” (CTest exit 8). A proposed resize-event
ordering change failed the same assertion and was reverted. Capturing the actual
displayed region before selection repairs the failure without changing boundary
rounding or weakening full-row requirements.

```sh
cmake --build --preset dev --target test_better_pull_review_qt_diff_workbench_tests test_better_pull_review_qt_diff_link_map_tests -j4
ctest --preset dev -R '^(qt_diff_workbench_scale_(100|125|200)|qt_diff_link_map_scale_(100|125|200)|architecture_boundaries)$' --output-on-failure
python3 _coding-standard/scripts/format.py --dry-run src/adapters/qt/src/diff_workbench.cpp tests/adapters_qt/diff_workbench_tests.cpp
python3 _coding-standard/scripts/tidy.py --build-dir build/dev --paths src/adapters/qt/src/diff_workbench.cpp tests/adapters_qt/diff_workbench_tests.cpp --warnings-as-errors
```

Build, format22 and both scoped tidy units pass. All seven affected integration
tests pass in 71.13 seconds on macOS with Qt 6.11.2. The unchanged controller
courses and 67 documentation verifier tests retain M2 evidence; the current
documentation structure gate is rerun after upkeep. Prior ENOSPC left a truncated
Ninja dependency log; `ninja -C build/dev -t recompact` repaired it before final
checks. No unrelated output was deleted. The official v003 freeze builds the
committed source inside its own root and validates its launcher before handoff.

## Post-v003 exact-flush correction

The owner supplied two before/after screenshots and explicitly required the
selected row's bottom to meet the view bottom. The preceding “less than one row
of slack” assertion could pass the rejected result; the new course requires flush
alignment after real key events to one device pixel. The unchanged v003 code
fails that assertion (CTest exit 8).

Removing source-boundary rounding alone also failed. A diagnostic measured row
bottom 374, `visibleRegion` bottom 380, actual mask bottom 364 and horizontal
scrollbar top 365. Qt's region included overscan behind the scrollbar, so the
v003 claim that this alone represented the displayed content was false. The
corrected captures intersect the region with the viewport mask. Regression
checks independently compare its mapped bottom with the native scrollbar top
minus one before requiring row-edge equality. Diagnostic instrumentation was
removed before final verification. The adaptive axis discriminator, native
fractional placement and accepted touchpad policy are unchanged.

Candidate `d97c8d105dcb57bbff49a1a5d948abf7f4012417` passes all seven
workbench/link-map/architecture checks on its exact frozen source/build at 100%,
125% and 200% in 48.23 seconds. The freeze included the application and both
affected test targets; its own launcher reaches the intended fixture file.
Because the candidate is read-only, CTest's generated registrations are copied
to an external writable control tree, changing only image output destinations.
The test commands, source, executable, fixture, scales, assertions and network
isolation remain frozen. The private `m4_exact_ctest.py` and
`m4-ctest-control.json` record that mechanical output relocation.

Format22, both scoped tidy units and the final documentation structure gate
pass. Unchanged controller and documentation-verifier evidence reuses M2.
An attempted stronger old-source negative rerun and main rebuild both failed
on unrelated concurrent dirty header/source-navigation code; those compile
failures are retained and are not regression proof. Candidate source was
restored before its explicit commit and isolated freeze. The fresh v004
attended session asks only for the remaining exact-flush keyboard judgment.


## Failure discrimination and diagnosis

The original M1 essential-dependency negative control forced `apply_frame`’s native
within-line displacement to zero while retaining the controller’s fractional
position. The official 100% workbench test then exits 8 at:

> sub-line input visibly moves the real source row before a line step

The original M1 source was restored byte-for-byte (SHA-256
`eb5af751f0579fb0cc2ff235e54b4dcaf55fbd64c35c9a0528f1f1a7b04c1ee5`)
and rebuilt before the passing integration rerun. This rules out a controller
position-only false success.

During diagnosis, a viewport height that changed with each residual allowed Qt
to reset native placement; fixed overscan corrected it. An exact End boundary
needed a floating-point tolerance in containing-row selection, and unified
reversal needed a stable ordinary-row stride instead of the final block’s bottom
margin. Replacement measurements now exclude viewport self-placement resize
events. Earlier integer-line scrollbar sweeps aliased fractional travel; the
revised sweeps inspect actual rendered origins and still assert exact endpoints
and ribbon agreement. These failures were corrected and the final courses pass.

The new keyboard course was run against the previous reveal behavior and failed
at “Down fully reveals even a partially clipped source row” (CTest exit 8).
During M2 correction it also caught an extra line of reveal: a transient
`visibleRegion` reported nine pixels of clipping where the stable content area
had five. Capturing the fixed mask before selection made that course pass, but
did not establish the actual displayed extent. The v002 owner rejection and new
fully-visible-target assertion falsified that conclusion; v003 replaces the
mask capture as described above. The axis negative control
applies the same 20:7 event to the original and revised discriminator: the original
returns `(-20,-7)` and fails, while the revised returns `(-20,0)` and passes.

## Scoped native diagnostic

The owner explicitly authorized computer use and input recording on their
prepared Sublime Text long-line document. A completed, stopped wizard captured
5,596 precise wheel events and 4,113 numeric viewport samples over 37.44 seconds.
An interior horizontal course delivered 353 vertical pixels with fixed viewport
Y. At another turn, movement resumed with recent 30 ms absolute travel around
18 Y versus 14 X, while gesture-to-date totals were 225 Y versus 801 X. This
supports recent directional evidence rather than whole-gesture accumulated parity.
Sampling and delivery delays leave the exact threshold uncertain; reversal alone
did not consistently release preference. These are observed trajectories with an
inferred policy, not closed-source algorithm identification.

The cardinal computer-use scroll API cannot reproduce precise simultaneous axes
or native phases. Scoped native event replay delivered movement but did not
reproduce the recorded trajectory, so replay fidelity and physical touchpad
equivalence are unproven. Original traces remain private outside Git. The
listen-only event tap, numeric viewport probe and wizard server are stopped.

The remaining uncertainty is the owner’s exact-flush keyboard reveal judgment. Other
operating systems and devices were not exercised as part of this bounded
milestone, and no native-device acceptance is inferred from synthetic input.

## Post-v004 horizontal scrollbar correction

The recorded v004 verdict is **Reject**: “The alignment/scroll looks fixed now.
But why does scrolling by tapping the down key bring up the horizontal scroll bar?”
The owner's observation does not override that verdict. The exact-flush geometry
and accepted adaptive touchpad discriminator are unchanged.

Qt 6.11.2's native [vertical value-change slot](https://github.com/qt/qtbase/blob/v6.11.2/src/widgets/widgets/qabstractscrollarea.cpp#L1376)
calls `flashScrollBars()`, which flashes both bars. BPR updates the hidden native
vertical bar to position source rows, so a Down reveal wakes the horizontal bar
even while its value remains unchanged. This is not a horizontal key dispatch.
The [native macOS style](https://github.com/qt/qtbase/blob/v6.11.2/src/plugins/styles/mac/qmacstyle_mac.mm#L5563)
uses `State_On` to keep a transient bar active.

Candidate `c66845fb99909b78a39e8d8d65c480ab77949ee0` retains the native
`QScrollBar` and its public `initStyleOption` hook. The source reports which axis
actually moved; only an unrelated `State_On` horizontal wake is suppressed.
Native hover, dragging and focus remain eligible. No private Qt API, forced
product style, scrollbar policy, origin, axis filter or timer is added.

The new test sends real Down events, renders the actual native scrollbar under
a deterministic transient probe style, and observes its value-change signals.
It requires no horizontal wake or value change during vertical navigation,
a visible wake after real horizontal wheel input, and Qt's return to transient
state. Both split panes and unified view, vertical pixel input and programmatic
vertical reveal are covered. The unchanged product fails the new assertion
(CTest exit 8, 7.09 seconds); the initial repair course passes (15.78 seconds).
The final exact frozen verification is in [M5 evidence](../issues/_checkpoints/smooth-diff-touchpad-panning/m5-verification.json).

A separate offscreen diagnostic loaded the actual macOS style plugin. Its
native pixel samples did not expose opacity changes after vertical input or a
1.7-second fade in either old or repaired control. Those samples are retained
privately and are **not** passing native visibility/fade evidence. The rendered
probe verifies Qt's real wake routing; only the owner can supply the remaining
native appearance judgment. No live desktop was driven or captured.
