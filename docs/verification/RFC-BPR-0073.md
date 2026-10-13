# RFC-BPR-0073 verification

The `smooth-comparison-panning` milestone implements the owner’s smooth-motion,
minor-drift suppression and discrete-keyboard amendments. Its immutable RFC base
is `268b87049592ca4643e5303ab77114e116e7b5f3`; its plan and import provenance are in
[the adopted plan](../rfcs/plans/smooth-diff-touchpad-panning/MILESTONES.md) and
[live import](../rfcs/assets/smooth-diff-touchpad-panning.LIVE_IMPORT.md).

Deterministic acceptance is complete for AC01–AC06 and AC08. AC07 remains pending
the owner’s verdict on the exact frozen build recorded in the
[continuation](../issues/_checkpoints/smooth-diff-touchpad-panning/README.md).
Fixture success does not establish physical touchpad feel.

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
clamping. A horizontal component at least four times the vertical component
suppresses vertical noise. One isolated small vertical perturbation stays
suppressed; two consecutive non-dominant events, or a vertical event of at least
eight pixels, release it. Discarded motion is never accumulated. Begin/end,
device and comparison/presentation changes reset context; an unphased event
after 250 ms also resets it. Forwarding preserves device, phase, source,
inversion and timestamp. These bounded constants remain subject to AC07.

Keyboard cursor/selection movement retains the fraction while the target is in
view. A keyboard reveal or page movement aligns the receiving pane through the
shared comparison authority. The other pane uses its normal projection through
unequal replacements. Unified scrolling uses its existing native row authority.

The refinement pass kept the shared origin, cached measurements and native
geometry together. Two optional assignments and six local test identifiers were
also made consistent with the static gate; those changes preserve behavior.

## Criterion evidence

| Criterion | Result | Direct evidence |
|---|---|---|
| AC01 | Pass | `test_fractional_native_viewport` and `test_fractional_unified_and_keyboard_viewports` send sub-line pixel motion to real source viewports. Native cursor rectangles and captured viewport pixels move before the line scrollbar advances, hold the residual and retrace accepted reversal. |
| AC02 | Pass | Controller course sends repeated horizontal deltas of about 20 pixels with ±1 vertical noise, starts at the horizontal edge, reverses direction, changes receiving pane and includes momentum/end. The shared vertical origin stays fixed. The unified composed course observes the actual source-row placement during horizontal motion. |
| AC03 | Pass | Small vertical-only input, sustained diagonal input and within-gesture reorientation are tested. Explicit device, phase, idle-unphased, comparison and presentation resets are covered. A viewport event observer verifies forwarded pointing device, phases and timestamp. |
| AC04 | Pass | Composed native geometry checks compare ribbon endpoints with source boundaries at fractional origins, verify actual partially clipped row identity and selection, and observe conversation-marker movement with the source. The same course runs at 100%, 125% and 200%, then repeats fractional ribbon checks after 150% code zoom. Existing gutter/mark and ribbon negative-control checks remain active. |
| AC05 | Pass | Existing controller/workbench/link-map checks retain insertion pinning, unequal replacements, Home/End, scroll-past-end, pane handoff, angle/Shift-wheel, modified zoom, reveals and reading-anchor restoration. Range sweeps now observe real rendered origins rather than aliased integer line-bar values; the exact endpoint remains asserted. One-pixel reversal from End is explicit. |
| AC06 | Pass | The composed 2,000-line course sends 1,000 accepted pixel inputs and observes 1,000 pixels of actual native source travel within one pixel. Counters require no semantic query/index adoption, geometry rebuild or pane scan; 1,000 frames coalesce to one repaint delivery. Link work stays within near-visible candidates and retained buffers do not grow. Existing publication, resize and input timing bounds remain active. |
| AC07 | Pending owner | One fresh short attended session presents the exact self-contained frozen build and the required Accept/Reject question. No implementer desktop driving or capture supplies this verdict. |
| AC08 | Pass | Both presentations retain an in-view fractional origin through Down and Shift-Down. Repeated arrows causing a reveal, page keys and endpoints align the receiving viewport; the next pixel starts at that origin. The controller separately verifies receiving-pane alignment through a 4:7 replacement without independently rounding its follower. |

## Commands and actual results

On macOS with Qt 6.11.2, all rendered checks use the offscreen platform. The
official CTest registrations run inside the repository’s network isolation.

```sh
cmake --build --preset dev --target test_better_pull_review_qt_diff_workbench_tests test_better_pull_review_qt_comparison_scroll_controller_tests test_better_pull_review_qt_diff_link_map_tests -j 4
ctest --preset dev -R '^(qt_diff_workbench_scale_(100|125|200)|qt_comparison_scroll_controller_scale_(100|125|200)|qt_diff_link_map_scale_(100|125|200)|architecture_boundaries)$' --output-on-failure
```

Build exited 0. All 10 integration tests passed, including the architecture
boundary check (50.83 seconds total). After the static-gate corrections and
negative control, the restored candidate was rebuilt and the nine rendered
integration tests were rerun successfully. The unchanged architecture result
is reused.

```sh
python3 _coding-standard/scripts/format.py --dry-run src/adapters/qt/include/bpr/qt/pixel_scroll_gesture.hpp src/adapters/qt/include/bpr/qt/comparison_scroll_controller.hpp src/adapters/qt/src/comparison_scroll_controller.cpp src/adapters/qt/src/diff_workbench.cpp tests/adapters_qt/comparison_scroll_controller_tests.cpp tests/adapters_qt/diff_workbench_tests.cpp
python3 _coding-standard/scripts/tidy.py --build-dir build/dev --paths src/adapters/qt/src/diff_workbench.cpp src/adapters/qt/src/comparison_scroll_controller.cpp tests/adapters_qt/diff_workbench_tests.cpp tests/adapters_qt/comparison_scroll_controller_tests.cpp --warnings-as-errors
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
git diff --check
```

Formatting uses clang-format 22.1.8. The four scoped translation units pass tidy
(the affected two were rerun after corrections; the unchanged two reuse their
passing results). Documentation reports `DOCUMENTATION_OK`; all 67
documentation verifier tests pass. Whitespace checks pass.

## Failure discrimination and diagnosis

The essential-dependency negative control forces `apply_frame`’s native
within-line displacement to zero while retaining the controller’s fractional
position. The official 100% workbench test then exits 8 at:

> sub-line input visibly moves the real source row before a line step

The original source was restored byte-for-byte (SHA-256
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

The remaining uncertainty is the owner’s physical touchpad judgment. Other
operating systems and devices were not exercised as part of this bounded
milestone, and no native-device acceptance is inferred from synthetic input.
