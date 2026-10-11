# BPR-1385 — Stable review bar

Owner mandate, 2026-10-10: file and work a corrective card for the comparison
summary and smaller file/change shifts during normal work without resizing.
Strategy: direct, no delegation. Selected readiness passed; the helper does not
verify host model loading. The original binding is retained in `initial-checkpoint.json`.
The final `checkpoint.json` renews the same assignment after adding the card's
completion evidence changed its body fingerprint; AC1–AC4 and the mandate are unchanged.
Candidate: `dc534e7142f9d804be7105d1d562397e20018491`; product/test digests are
in `source-hashes.json`.

## Implementation and scope

The released comparison button reserves a native-font-sized slot for comparison
names, dates and readiness. The compact slot preserves the ordinary loading stage
and received-page count. File and change counters reserve four digits per number,
or three in compact mode, even without a selection. Their larger values and longer
status sentences elide inside those slots and retain complete tooltip and
accessible descriptions. The Updates disclosure reserves the warning mark's room
when the mark is absent. Actual window/appearance changes still choose the existing
date, compact-position and label-only forms.

UX work used `frontend-design:frontend-design`, loaded from
`/Users/jaeger/.claude/plugins/marketplaces/claude-plugins-official/plugins/frontend-design/skills/frontend-design/SKILL.md`.
Design direction: retain the native palette, fonts, focus painting, group rails
and control order; reserve geometry for changing text. No new palette, icons or
typography were introduced. The inspected renders keep the labels legible and
the arrows in their reserved positions.

The change is confined to Qt presentation. Core decisions, host effects,
provider/credential authority, persistence, threading and command dispatch have
no semantic change. The owning UX contract is updated in
`docs/ux/workbench-specification.md`. Existing broader RFC native/human obligations
are unaffected; this bounded, measurable correction has no new human criterion.

## Regression and acceptance

The new geometry course failed against the original toolbar source at
`068db28fd76b02a0a5ba9dd095f2a1c8370b13ba`:
`qt_workbench_title_row_scale_100` reported
“readiness and recovery preserve the comparison slot and every navigation rectangle”.
The three production files were unchanged by the concurrent BPR-1384 commits.

The first integration pass caught a compact-summary regression: its loading
page count was elided. Increasing that reservation initially displaced compact
file positions at the width floor. The final sizing preserves the loading count
and uses a three-digit compact counter reservation, satisfying both existing
assertions without weakening them.

- AC1: Fixed-size geometry equality across short loading, unavailable history,
  long preparation, recovery and all three named comparisons; full tooltip and
  accessible readiness; visible menu and strong keyboard focus.
- AC2: Unselected and selected files through 1, 8, 9, 10, 11, 99, 100 and 101;
  computed comparisons containing 1, 11 and 101 separated groups, navigated to
  their last group; loading/replacement and both warning-mark states. Every
  comparison, counter and arrow rectangle is unchanged at each fixed width.
- AC3: Existing resize sweeps, date/word/number yield order, no toolbar overflow,
  enlarged-font floor and restored-geometry courses; inherited appearance.
- AC4: Fail-before/pass-after regression; the full composed application loads an
  exact Git/cache comparison, selects a file with a missing endpoint, reports its
  failure and recovers the readable file with identical summary/arrow rectangles.
  Full-window offscreen renders accompany those states.

## Verification commands

Run on macOS, Debug, Qt 6.11.2, offscreen. The scale courses use 1.0, 1.25 and 2.0.

```sh
cmake --build --preset dev --target test_better_pull_review_qt_workbench_window_tests test_better_pull_review_qt_github_tests test_setup_destination_public_path test_better_pull_review_qt_epoch_comparison_tests -j 2
BPR_QT_RENDER_PATH=/Users/jaeger/Documents/GitHub/BetterPullReview/build/dev/bpr1385-evidence/review-bar.png BPR_SETUP_ARTIFACT_ROOT=/Users/jaeger/Documents/GitHub/BetterPullReview/build/dev/bpr1385-evidence ctest --preset dev -R '^(qt_workbench_title_row_scale_(100|125|200)|qt_epoch_comparison_scale_(100|125|200)|qt_workbench_window_scale_100|qt_composed_github_scale_100|application_selected_file_safety_public_path)$' --output-on-failure -j 1
ctest --preset dev -R '^architecture_boundaries$' --output-on-failure
python3 _coding-standard/scripts/format.py --dry-run src/adapters/qt/src/epoch_comparison_control.cpp src/adapters/qt/src/workbench_strip.cpp src/adapters/qt/src/workbench_window.cpp tests/adapters_qt/workbench_window_tests.cpp tests/end_to_end/setup_destination_public_path_tests.cpp
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
git diff --check
```

The final nine-test selection passed (76.68 seconds). Architecture
boundaries passed before the final width-only adjustment (26.70 seconds); its inputs concerning
layering are unchanged. Clang-format 22.1.8 passed for all five touched C++ files.
The documentation gate printed `DOCUMENTATION_OK`; its 67-test suite passed.

Local renders are under `build/dev/bpr1385-evidence/`: summary error/ready at 1280
and 900, plus `stable-review-bar-source-ready.png`,
`modified-missing-endpoint.png` and `stable-review-bar-source-recovered.png` from
the full application. Screenshots and build output are not committed. Offscreen
evidence does not claim native Windows/Linux, screen-reader, installed-build or
release qualification. No live GitHub, owner desktop or browser data was needed.

Self-review: the slots use font/style measurements and existing responsive modes;
they do not retain an expanding historical maximum, change navigation semantics,
or introduce a second layout authority. Geometry checks verify actual widgets,
and existing integration checks preserve compact information and commands.

## Closeout

AC1–AC4 pass. `acceptance.json` records the evidence per criterion;
`observation.json` and `checkpoint-result.json` preserve the direct milestone's
completion and quiescence. No task-owned command or writer remains. No protected
candidate, review server or new worktree was created. Unrelated `docs/proposals/`
is preserved. The next step is to report the completed correction; no RFC-wide
closeout or installed-app replacement is claimed.
