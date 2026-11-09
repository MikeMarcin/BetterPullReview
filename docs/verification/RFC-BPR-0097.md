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

## Owner appearance revision r2

The optional attended review of exact frozen v002 was handed back at37.
Navigation and focus were accepted; dialog appearance was rejected. The owner
requested exclusive Left/Right chips inspired by Find in Files and less wasted
space. Mandate amendment `go-to-line-appearance-r2` retains all original
AC01–AC04. [Reconciliation and current handoff](../issues/_checkpoints/go-to-line-v1/post-review.md).

Appearance source/test candidate: `df4ccfc8587e203048eb3d74499f6305480e5720`.
The shared chip is extracted from Find in Files with its existing filter
callback intact; Go to line uses native QButtonGroup exclusivity. The dialog
sizes to its controls and hides the unused validation row. The composed
appearance owner supplies the same semantic chip fills and refreshes an open
dialog on appearance publication. Exact-line reveal dispatch is unchanged.

The corrected shortest composed course passed in 24.60 s. It additionally
exercises Left/Right arrow selection, one active side, native Tab traversal
and accessible endpoint names/checked state. The initial build needed the
QComboBox include retained for historical-comparison test controls. The first
course exposed an outdated assumption of two Tab stops; Qt’s native exclusive
group has one stop and arrow selection. Both diagnostics are preserved in
external logs; the corrected course passed before freezing.

Focused clang-tidy22 and format22.1.8 pass, as do documentation structure and
all 67 documentation controls. Command logs are external at
`/tmp/bpr1948-r2-*.log`. No owner desktop was driven or captured. New appearance
acceptance remains pending; the accepted v002 installation’s binary hash was
verified unchanged.

Final focused serial integration passed **6/6** on the exact source/test
candidate: architecture boundaries 19.95 s, architecture contract 223.59 s, composed
Go to line / source Find 26.20/27.97/26.23 s at 100/125/200%, and Find in Files 19.13 s; total 343.11 s. The exact command log is
`/tmp/bpr1948-r2-integration.log`.
The full-window split valid/invalid, 125% unified valid and 200% unified invalid
rasters were inspected; controls and feedback fit the minimum 957×360 logical
window. The composed course also asserts the short-file valid dialog stays
under 200 logical pixels high with no empty validation row. This verifies layout
and interaction, not native system appearance. No dedicated Debug timing course was selected.

The fresh frozen review is recorded in the post-review checkpoint.

## Owner appearance revision r3

The owner rejected exact v003 at handback 8 and requested a dedicated Astra max
design pass. The supplied screenshot showed a path heading controlling width,
isolated range text and a separate button footer. Frontend-design and its
desktop guidance informed the replacement: one aligned control row and one
quiet file/range row. Native buttons and their platform order, exclusive side
chips, line validation and navigation dispatch are retained. Long paths elide
without changing the panel width; full identity remains accessible and in the
tooltip. [Current candidate and handoff](../issues/_checkpoints/go-to-line-v1/post-review.md#appearance-r3-design-pass).

The initial real-composition dark preview exposed that top-level dialogs do not
inherit a workbench's explicit profile palette by default. The panel now uses
the repository's existing `Qt::WA_WindowPropagation` convention to inherit
font and palette. The composed course checks construction in a dark profile
and a subsequent light-profile publication while the dialog stays open.

Source/test candidate: `fed02076b58d210b2a7ad09ad069364aac555ecb`.
The final source/test candidate passes the complete existing Go to line and
source Find public course at 100/125/200%: **3/3**, 24.45/21.61/22.95 seconds,
69.05 seconds total. Its keyboard, exact target, cancellation, stale-request,
exclusive-side, unavailable-side and bounded-work checks are unchanged. It
now also retains panel rasters and requires the valid dialog to remain below
110 logical pixels in height. The preceding layout-only four-row integration,
including `architecture_boundaries`, passed in 88.11 seconds; adding window
palette propagation changes no dependency boundary. Existing unchanged
architecture-contract and Find in Files evidence is reused.

Format22.1.8, focused clang-tidy22, documentation structure and all 67
documentation controls pass. The only source owner is the Qt dialog; the
shared side chip, application and core are unchanged. No dedicated Debug
timing course was selected; these durations are functional-course observations.

External evidence is under
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-go-to-line-r3-mrxgnanc`:
`palette-integration.log`, `palette-tidy.log`, `documentation-tests.log` and
`docs.log`. A disposable renderer links the current production composition
and opens the same committed `inline-drawer-v1` fixture and 1,557-line file as
the owner look. It performs the actual Go to line to line 608, then captures the
reopened dialog in dark/light and invalid states. It runs with network denied,
an owner-private temporary profile and `QT_QPA_PLATFORM=offscreen`; its only
position adjustment centers the modal because the offscreen platform has no
window manager. It leaves no product seam or tracked output.
All four final views were inspected: the valid panel is 427×72 logical pixels,
and validation expands it to 95 pixels. Full path and range fit together on
the actual 1,557-line fixture. Final images are in `inline-drawer-renders-final`
under that external root.

The final full-window and panel previews are layout evidence. A diagnostic
macOS-style offscreen run passed the interaction course but failed to draw
native edit/button chrome; that raster is not used as visual evidence. Native
system appearance remains for the owner to judge in the new frozen candidate.
No owner desktop or personal browser data was accessed, and no remote mutation,
push or release occurred. Prior v002 navigation acceptance and v003 rejection
remain bound to their exact frozen builds.


## Column and compact-panel amendment (BPR-1957)

Owner mandate `go-to-line-columns-r4` adds RFC AC05–AC06. Original BPR-1948
acceptance remains bound to its earlier producing revisions. The new panel
reuses Find's exact close control, removes filename and Cancel, and accepts
plain lines or one-based native UTF-16 `line:column`. Tabs count once,
supplementary characters twice, CRLF does not count, and end-of-line is allowed.
Invalid and half-surrogate positions are refused rather than clamped. The
current line's column range is visible and accessible. Column navigation
preserves the exact single-source-line comment anchor.

The final integration passes **4/4, 139.48 s**: `architecture_boundaries` and
`application_source_find`, `application_source_find_scale_1_25`,
`application_source_find_scale_2_0`. These composed checks exercise menu and
physical key, both presentations/sides, valid and invalid column syntax,
side-dependent column limits, CRLF/supplementary Unicode, exact native caret,
subsequent draft, cancellation, stale material, accessible Close and range,
light/dark propagation, minimum-size layout and unchanged bounded-work counters.
Their durations are 47.29/21.46/23.22 s; no dedicated performance course ran.

The production Qt workbench's `--go-to-line-only` course passes at
100/125/200%, using each registered scale's network-denied command and environment.
It covers an empty source line's sole caret position, tabs, surrogate boundaries,
zero-based native offsets corresponding to one-based input, CRLF, end-of-line,
invalid-input nonmutation and horizontally overflowing line visibility, as well
as the original unavailable/nontext states. These are native widget observations,
not parser-only assertions. The first composed run failed a test-only expected
maximum of 11 for an 11-unit UTF-16 fixture line; the correct maximum is 12.
After fixing that assertion, all final courses pass without a product correction.

Format 22.1.8 and tidy 22 on all three changed translation units pass;
documentation structure and all 67 documentation controls pass. Logs are under
the external r3 evidence root above, named `columns-integration.log`,
`columns-native-focused.log`, `columns-native-125.log`, `columns-native-200.log`,
`columns-format.log`, `columns-tidy.log` and `columns-doc-controls.log`.
The actual `inline-drawer-v1` composition renders in `columns-renders` show
400×72 logical-pixel valid panels and 400×95 invalid panels in light/dark.
No owner desktop was accessed. Native appearance remains pending the exact
v005 frozen owner review; neither prior rejection nor v002 navigation acceptance
is transferred to the new candidate.


Final source/test change: `996aa30696978004f6b0f0bb0b17dd206b606088`;
frozen revision `dd968668313c5abe343273de960ea9f00bfefae1` differs only by
claim bookkeeping. The exact self-contained v005 build passed its own launcher
check at the expected file and fixture head. AC05 passes; AC06 appearance is
pending in short session `9827b9b1187675faf91e0e097db5a23f`, consumed cursor 4.
The [handoff](../issues/_checkpoints/go-to-line-v1/review-session-v005.json)
records binary hash, protected fingerprint, external state and host ownership;
no authenticated URL is tracked. The earlier unused v004 freeze completed,
was superseded before init/serve, and was removed through the freeze helper.
