# BPR-1197: copyable comparison status checkpoint

## Assignment and candidate

- Coordinator mandate: the 2026-09-25 follow-up for the two deterministic
  RFC-0052 status failures, amended to use reserved BPR-1197. Qt status
  presentation, affected tests and documentation only; no primary integration,
  push, application/cache changes or frozen attended-candidate changes.
- Base: `f6de40b633ea8b879d3643ad5a259169fd976975`.
- Worktree: `/Users/jaeger/.codex/worktrees/bpr1132-status-refusal/BetterPullReview`.
- Branch: `codex/bpr1132-status-refusal`.
- Product: `24cee6dfc85fec9c4d1d107bd6528ebf81b381df`.
- Runtime/evidence: `/Users/jaeger/.codex/tmp/bpr1132-status-b8w8qmsa`.
- Environment: macOS 26.6.2 arm64, Qt 6.11.2, Debug, offscreen, serial GUI tests.

The product fixes the full explanation's visibility and literal rendering when
the comparison has no text, and gives the compact tab status enough minimum
width for its ellipsis and tooltip target. Successful text comparisons retain
the compact header. No source/provider/request policy changes and no composition
pin changes are involved. Self-review covered the status observer's initial,
clear, unavailable, binary and normal-text transitions, the label's selection
and accessible description, and the status painter's padding/size relationship.

## Verification and failures retained

| Evidence under runtime `evidence/` | Result |
| --- | --- |
| `configure.log`, `build-before.log` | Isolated offline configure/build; primary dependency cache cloned with APFS copy-on-write into this worktree |
| `tests-before.log` | Original product: both failures reproduced, 56.44 s |
| `tests-test-regression.log` | Strengthened tests with unchanged product: both public paths and scale 100 fail, 42.72 s |
| `tests-after.log` | Initial fix: both public paths, all window/focus scales and all four geometry profiles pass; three new tab-scale width assertions fail, 9/12 overall |
| `geometry-diagnosis.log` | Status width 137 at window 1280, zero at window 1100; strip remained 28 high |
| `tests-status-width.log` | Minimum ellipsis width correction: tab scales 100/125/200 pass |
| `build-final.log`, `tests-final.log` | Final committed product source: build passes, 7/7 checks pass in 124.32 s |
| `format-final.log` | Six changed C++ files pass |
| `tidy.log`, `tidy-status-width.log` | Five original changed translation units plus final status-strip correction pass; existing naming/style diagnostics remain unchanged. Subsequent cleanup removes the temporary geometry trace and redundant initial hide, adds explicit key-sequence includes and strengthens the width assertion |
| `docs-before-commit.log`, `docs-tests.log` | Documentation gate and 64 regression tests pass; gate repeated after finish |

The final seven rows are `architecture_boundaries`,
`qt_document_tabs_scale_100`, `qt_document_tabs_scale_125`,
`qt_document_tabs_scale_200`, `application_unavailable_diff_public_path`,
`application_selected_file_safety_public_path`, and
`application_uiforetw_selected_diff_contract`. The six unchanged window/focus
scale passes from the first correction run are retained. The final geometry
course measures **872 folded / 838 open**, with a **28-pixel tab strip**, for
light, dark, high-contrast light and high-contrast dark at 1280×1000.

Both public paths select the complete label text, deliver the platform Copy key
through Qt and assert the complete clipboard contents. They assert visible
QAccessible descriptions, stale-source clearance and successful recovery. The
missing-endpoint path retains its full wrapped-height assertion. The tab course
checks the separate compact label, including full tooltip/accessible text,
legitimate elision geometry, literal full explanation and recovery at 1280,
1100 and the macOS 900-pixel floor at all three scales. These are automated Qt
observations, not native screen-reader acceptance.

### Inspected renders and artifact identities

- Before: `/Users/jaeger/.codex/tmp/bpr1132-status-b8w8qmsa/evidence/before/modified-missing-endpoint.png`.
  It has empty source panes and an elided tab status, without the full selectable
  explanation. SHA-256 `f7445e709c2bc755980d0a3c1cf4a6c2ae8e6513fcaf4b5b0dcef86c4466a875`.
- Final missing material: `/Users/jaeger/.codex/tmp/bpr1132-status-b8w8qmsa/evidence/final/modified-missing-endpoint.png`.
  The complete reason wraps visibly above the empty source panes.
- Final comparison limit: worktree `build/dev/tests/end_to_end/unavailable-diff-window.png`.
  The complete selectable reason is visible; binary and exact-text recovery
  follow in the same test.
- Narrow full explanation: worktree
  `build/dev/tests/adapters_qt/document-tabs-100-document-tabs-unavailable-900.png`.
  The whole literal text wraps, and the header retains an ellipsis target.
  The corresponding 125/200 and 1100/1280 renders are beside it.
- Four final measured geometry records: worktree
  `build/dev/tests/end_to_end/uiforetw-selected-diff-contract/data/evidence/*/capture-facts.json`.
- Runtime `verification.json` records exact artifact paths and SHA-256 hashes.
  Build/test sources match the product commit; the binaries were built from
  its uncommitted source immediately before that commit, with build HEAD
  `73ea3a464de405eb20e6eb0ce3bc5e4a4c2504c1`.

## Reproduction commands

Run from the isolated worktree above. Its cache is private; no primary or frozen
candidate path is written by these commands.

```sh
cmake --preset dev -DBPR_DEPENDENCY_OFFLINE=ON
cmake --build --preset dev -j6 --target test_setup_destination_public_path bench_better_pull_review_large_review test_better_pull_review_qt_workbench_window_tests app_better_pull_review
BPR_SETUP_ARTIFACT_ROOT=/Users/jaeger/.codex/tmp/bpr1132-status-b8w8qmsa/evidence/recheck ctest --test-dir /Users/jaeger/.codex/worktrees/bpr1132-status-refusal/BetterPullReview/build/dev -j1 --output-on-failure -R '^(architecture_boundaries|qt_document_tabs_scale_(100|125|200)|application_unavailable_diff_public_path|application_selected_file_safety_public_path|application_uiforetw_selected_diff_contract)$'
python3 _coding-standard/scripts/format.py --dry-run src/adapters/qt/src/diff_workbench.cpp src/adapters/qt/src/document_tab_strip.cpp src/adapters/qt/src/workbench_window.cpp tests/adapters_qt/workbench_window_tests.cpp tests/end_to_end/large_review_benchmark.cpp tests/end_to_end/setup_destination_public_path_tests.cpp
python3 _coding-standard/scripts/tidy.py --build-dir build/dev --paths src/adapters/qt/src/diff_workbench.cpp src/adapters/qt/src/document_tab_strip.cpp src/adapters/qt/src/workbench_window.cpp tests/adapters_qt/workbench_window_tests.cpp tests/end_to_end/large_review_benchmark.cpp tests/end_to_end/setup_destination_public_path_tests.cpp
```

This new worktree has no local MASK deployment. Documentation commands used
`PYTHONDONTWRITEBYTECODE=1` and `PYTHONPATH` set to the installed skill's scripts:
`/Users/jaeger/.mask/store/v1/6ad19f0fc539b9b393a02dbeff6bf0745f83ac499d7c953cba4d9236bf667832/payload/implement-rfc/scripts`.
With those environment variables, `python3 scripts/verify_documentation.py
--root .` and `python3 tests/scripts/verify_documentation_tests.py` pass. The
first unconfigured documentation attempt reported missing `board_model`; no
global MASK deployment was changed.

## Integration and ownership handoff

Cherry-pick, in order:

1. `bd736fa2aeb6c63d9c3e4dec71acf915e5562130` — plan BPR-1197.
2. `73ea3a464de405eb20e6eb0ce3bc5e4a4c2504c1` — claim BPR-1197.
3. `24cee6dfc85fec9c4d1d107bd6528ebf81b381df` — product, tests, specification and closeout dependency.
4. The BPR-1197 finish commit containing this checkpoint — acceptance and Done move.

Skip these reservation imports, already present on main: isolated `53baf51e`
duplicates main `d50f2f03`; `44d7505e` duplicates `cbf42935`; `684ebdc4`
duplicates `480af68d`. Do not integrate those duplicate counter/receipt changes.
The only shared closeout-card edits add BPR-1197 to BPR-1135's dependency and
planned-card lists; union them with independently added follow-ups such as
BPR-1198. This slice changes no `github_application.cpp` or application
`document_tabs.cpp` hunk.

The coordinator owns primary integration and any push. All task-owned builds
and tests are complete at handoff; no server was started. Temporary geometry
instrumentation was removed. The isolated build and evidence are retained for
inspection. No primary checkout, frozen m2 app, attended state, event cursor,
server or watch was changed. The isolated writer is released after the finish
commit and final documentation gate.

## Remaining obligations

BPR-1197 is complete on macOS. BPR-1135 retains Windows/Linux execution, native
VoiceOver/Narrator, the owner's existing m2 decisions and the remaining RFC-0052
closeout work. The existing frozen attended candidate does not contain this
follow-up; its source identity and outstanding decisions are unchanged. No
human acceptance or cross-platform result is inferred from this checkpoint.
