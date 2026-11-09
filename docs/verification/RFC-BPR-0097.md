# RFC-BPR-0097 verification

Mandate `go-to-line-r1`: implement the supplied RFC-DRAFT-0030 unchanged,
all AC01–AC04, as milestone `exact-source-line` / BPR-1948. There is no
required human, native or independent acceptance criterion.
RFC_BASE_SHA: `663137a9ed3825c87934c93926d414725f8bee22`.
Product candidate: `52c8469b6a1c9d5fd9fc0fe7a6c39469abc73841`.
The final test candidate adds native-cursor content and unified-scroll
cancellation assertions; product bytes are unchanged. Its exact revision and
final focused results are recorded below.
Final test revision: `e3e600c9f5483bcec66b140d0473d232b4ae7564`.
Its added native-cursor and unified-cancellation checks pass at all three scales:
22.40 / 21.64 / 22.49 seconds (66.58 seconds total). The prior 13-row integration
remains applicable because this revision changes only those assertions. Reservation-only merge `7563490e9`
retains concurrent main allocations without changing that candidate's product,
tests or configuration. Frontend-design and its desktop guidance were loaded.

## Evidence and results

| Criterion | Direct check and result |
|---|---|
| AC01 | The composed View action and physical platform key open the same window-modal dialog. The resident menu mirrors its disabled item and key; Navigation help finds the catalogue entry. Home, Overview and other PR modalities disable it. A real inline draft retains its key handling, while explicit menu invocation preserves its words and Cancel restores its focus. The catalogue keeps macOS Command-G / Command-Shift-G Find bindings. All three shortcut catalogue/help courses pass. |
| AC02 | Through the production composition and installed Git/cache, both source sides and both presentations select source lines 2 (inside the two-line changed group), 501 (unchanged source outside hunks), and 1107 (last line). Semantic selection, single-line draft range, source focus and visible target agree. Scrolling the already selected last line away and invoking again reveals it. Up continues from that row; unchanged unified rows retain their existing canonical Head identity when ordinary keyboard traversal chooses the next row. Return opens an actual private comment draft anchored on requested Head line 501. A retained base/base comparison navigates its own displayed endpoints. |
| AC03 | Empty, zero, negative, fractional, whitespace-prefixed, overflowing and out-of-range input disable Go with readable validation and no selection/frame change. Side changes retain the number. Escape and Cancel preserve reading state. Added and emptied files select the sole navigable side and disable the other choice; an empty source disables the command. Existing Qt nontext courses now also assert binary, refused and loading destinations are unavailable. Replacing the file, duplicating the tab with identical material or selecting a historical pair while the dialog is open retires its request; Return cannot apply a stale file request. |
| AC04 | The composed keyboard/menu course traverses Side and Line, verifies initial numeric selection, accessible names/range descriptions, ownership, focus and fitted controls at the actual minimum 957×360 logical window. Full-window dialog, invalid-input and hidden-number target rasters are produced at 100%, 125% and 200%. Source publications, semantic-index adoption, pane measurements and Find matching remain unchanged during navigation; installed-Git diff-helper and prepared remote-source counters also remain unchanged. Split reveal uses the shared controller and indexed segment lookup; unified reveal uses its retained source-to-row map. |

Final serial integration: **13/13 pass**, 334.83 seconds. This includes both
architecture checks; Qt diff workbench and keyboard shortcut/help courses at all
three scales; composed header/source navigation; composed Go to line + source
Find at all three scales; and composed PR Find after extending its shared fixture.
The source-Find rows took 24.51 / 21.80 / 22.34 seconds. These are local
functional-course observations, not performance claims. No dedicated Debug
timing course was selected.

Environment: macOS 26.6.2, arm64, Qt 6.12.0, Debug, offscreen Qt; installed Git
and the normal private cache actor. `source-find-v1` retains its 1,107-line
Base/Head source and adds deterministic added, emptied and empty-file cases.
Its committed pack, recorded responses and composition's pinned OIDs agree.
No remote source or owner desktop was accessed.

The 100% split/unified dialogs and target, 125% invalid dialog and 200% unified
hidden-number target were visually inspected. The controls, readable validation,
disabled Go and selected source band fit. Offscreen rendering establishes layout
and deterministic interactions; it does not establish native system appearance,
VoiceOver behavior, Windows/Linux execution, Release performance or package trust.
Those are outside this RFC's required acceptance.

## Commands and artifacts

External command logs live under `/tmp/bpr-go-to-line-run`. Full-window rasters
are ignored artifacts in `build/dev/source-find-renders`,
`source-find-renders-1_25` and `source-find-renders-2_0` (`line-*.png`).
No logs, rasters, build output or credentials are tracked.

```sh
cmake --preset dev -DCPM_SOURCE_CACHE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache
cmake --build --preset dev --target test_source_find_public_path test_source_pr_find_public_path test_header_source_public_path test_better_pull_review_qt_diff_workbench_tests test_better_pull_review_qt_workbench_window_tests --parallel 6
ctest --test-dir build/dev -R '^(architecture_boundaries|architecture_boundary_contract|qt_diff_workbench_scale_(100|125|200)|qt_keyboard_shortcuts_scale_(100|125|200)|application_source_find(_scale_(1_25|2_0))?|application_source_pr_find|application_header_source_navigation)$' --output-on-failure
python3 _coding-standard/scripts/tidy.py --build-dir build/dev --paths src/adapters/qt/src/workbench_window_line_navigation.cpp src/adapters/qt/src/diff_workbench_marks_and_navigation.cpp src/adapters/qt/src/diff_workbench_presentation.cpp src/adapters/qt/src/keyboard_shortcuts.cpp --warnings-as-errors --jobs 2
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
git diff --check
```

Clang-format 22.1.8 checks all 16 changed C++ files; focused clang-tidy 22
passes all four selected production translation units. Both documentation
commands pass (67 control tests). A concurrent main reservation advanced the
counter to BPR-1951; merging its existing commit corrected the initial
historical-high-water refusal without rewriting another task's allocation.

## Diagnosis and falsification

Initial harness compilation needed complete Qt includes and the controller's
actual API names. The additional native-cursor assertion also needed an explicit `QTextBlock`
include; a prematurely launched run against the previous binary was discarded.
The first runtime crash was the harness retaining a
`WA_DeleteOnClose` dialog reference after Return; it now uses `QPointer`.
The offscreen plugin has no window manager to reactivate the modal's owner;
the harness restores only owner activation and then checks the product's
chosen focus. Directly setting presentation on the child was replaced with
production profile-owned menu actions so projections cannot undo the test's
layout. An over-strict unified Up expectation was corrected to its existing
canonical shared-row behavior, while the exact requested Base identity is
still asserted immediately after Go.

Go retains its exact anchor across pending geometry measurement, clears older
presentation/restoration anchors and replaces native ranges before reporting
selection. The fixture generator's binary-file experiment could not verify
Git's `-` numstat result; that addition was removed, and binary refusal is
covered by the existing real nontext Qt publication course.

A negative control removes the dialog's navigation dispatch while leaving its
controls intact. The composed course fails at the exact source-side/line
assertion (4.02s), observing Base line 1 instead of requested Base line 2.
The original bytes were restored in `finally`, rebuilt, and the composed course
passed again (22.79s). The first control mutation was refused by the compiler
for an unused lambda capture; retaining the inert capture allowed the runtime
control to execute. No negative-control switch is part of the candidate.

## Closeout

A concurrent main import also selected the original isolated RFC-BPR-0096 ID.
This milestone was renamed RFC-BPR-0097, preserving that other active pack,
BPR-1948, all four criteria, original provenance and every product/test byte.
The migration receipt and renewed binding are in the checkpoint directory.

All four criteria are satisfied by the bound evidence. The
[checkpoint](../issues/_checkpoints/go-to-line-v1/handoff.md) retains commits,
selection, binding, changed paths, command state and continuation. No attended
session, push, release or external publication is part of this work.

The owner subsequently requested an additional attended UX look. Its fresh,
prepared session is recorded in the
[review handoff](../issues/_checkpoints/go-to-line-v1/attended-review.md).
The existing automated acceptance is unchanged; supplemental owner decisions
remain pending for their exact frozen candidate.
