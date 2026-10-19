# RFC-BPR-0087 verification and continuation

## Current candidate

- RFC import: `6fcdd1537`, based on immutable RFC_BASE_SHA
  `74ef7f4fb643b2fbbcd4753efcd1fe29c38bbddc`.
- First source slice: `2660e4c63096479bb5d78090017e9f0e8d898e28` on
  `codex/shared-action-buttons`.
- Isolated worktree:
  `/Users/jaeger/Documents/Codex/2026-10-19/task-4/shared-action-buttons`.
- The delegated implementer used `author-rfc`, `implement-rfc` and the required
  `frontend-design:frontend-design` skill. The design follows BPR-1577's accepted
  v002 chip; no new visual direction was substituted.
- BPR-1659 is Blocked on `disk-space`. The user's disk-space steering paused
  large builds/downloads before any were started. `df -h .` observed about
  1.0–1.1 GiB available on the Data volume. This task started no build process,
  download, frozen candidate or large generated output. No task-owned command
  remains active at handoff. The source worktree must be preserved.
- Root README belongs to the separate README task and was excluded from all
  commits. No existing release worktree, release artifact, merge, push or
  release was changed by this implementation.

## Written source, not yet runtime accepted

`button_surface` now owns the palette-derived chip, hover/press and keyboard
focus treatment. Overview retains its mark/content and measured geometry.
`quiet_tool_button` adopts the same surface for Review/Comments option and Reset
factories and Appearance tab-width choices. Native menu, arrow, icon-only and
stacked icon/text variants retain their own painting. QPushButton migration is
future work, not a tool-button substitution.

The [inventory](../ux/shared-action-buttons.md) covers the 46 translation units
with direct push/tool-button construction or page-local subclasses and names
the separately declared specialized classes. Pending entries remain BPR-1660's
work. The RFC and first milestone remain open.

The existing `qt_feedback_label_settings` course now includes a composed
four-theme check of the new Settings choices: selected/bare/Reset pixels,
hover and press, keyboard focus and Space, exclusive dispatch, disabled
rendering and refused disabled clicks. This test source has compiled in
syntax-only mode, but has **not run**. Its pixel thresholds and runtime
assumptions still need verification. The existing Overview switch test remains
the regression check on the accepted reference.

## Lightweight checks actually performed

On macOS arm64 with the existing Qt 6.11.2 dependency headers:

| Check | Actual result |
|---|---|
| Syntax-only compiler check of `button_surface.cpp`, `quiet_tool_button.cpp`, `pull_request_overview_page.cpp`, `appearance_settings_panel.cpp`, `workbench_window.cpp` and `feedback_label_settings_tests.cpp` | All six exit 0 with the existing dev compile flags, including warnings as errors. Commands were derived from the main dev compilation database, remapped to this worktree, with `-c`/object output removed and `-fsyntax-only` added. This is not a build/link/runtime pass. |
| `python3 _coding-standard/scripts/format.py --dry-run` on all eight touched C++ headers/sources | Pass, clang-format 22.1.8. |
| `python3 _coding-standard/scripts/tidy.py --root . --compile-commands /Users/jaeger/Documents/Codex/2026-10-19/task-4/syntax-checks/compile_commands.json --paths src/adapters/qt/src/button_surface.cpp src/adapters/qt/src/quiet_tool_button.cpp --warnings-as-errors` | Pass, clang-tidy 22. Initial pragma-once findings were corrected to include guards; final run has no first-party findings. |
| `cmake -DBPR_ROOT=/Users/jaeger/Documents/Codex/2026-10-19/task-4/shared-action-buttons -P cmake/VerifyArchitectureBoundaries.cmake` | Pass: application, provider and Qt architecture boundaries verified. |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK`. |
| `git diff --check` | Pass. |
| Shared milestone-plan check and checkpoint binding | Structurally valid; bound to `shared-settings`. Two missing later-milestone boundary fields were corrected without changing scope or exits. Structural validity does not certify semantic acceptance. |

## Acceptance frontier

| Criterion | State |
|---|---|
| AC01 | Source extraction and first Settings adoption written; actual rendered behavior pending. |
| AC02 | Initial inventory written; app-wide adoption explicitly incomplete. |
| AC03 | Existing Qt interaction preserved by design and new regression source written; runtime interaction and accessibility checks pending. |
| AC04 | No new runtime renders inspected. Four themes, three scales and style variants remain pending. Native Windows/Linux and macOS appearance integration are unobserved. |
| AC05 | Lightweight checks above pass; focused build, runtime tests and composition checks remain pending. |

## Resume after disk headroom is confirmed

Keep using this worktree. Do not rebuild inside another task's tree or use a
previous executable as evidence of this source. Configure an isolated dev
build using the already available dependency cache where appropriate; do not
start another large download on the full disk.

Build the affected test targets, then run focused tests serially:

```sh
cmake --build --preset dev --target test_better_pull_review_qt_feedback_label_settings_tests test_better_pull_review_qt_pull_request_overview_page_tests test_better_pull_review_qt_appearance_settings_panel_tests test_better_pull_review_qt_settings_window_tests test_better_pull_review_qt_workbench_window_tests
ctest --preset dev --output-on-failure -R '^(qt_feedback_label_settings|qt_pull_request_overview_page|qt_appearance_settings_panel|qt_settings_window(_scale_(125|200))?|qt_workbench_window_scale_(100|125|200))$'
```

Also build/run `qt_composed_github_scale_100` as the first composition-focused
integration check. Inspect the real Settings Review/Comments/Appearance and
Overview captures through the test render hooks, including the new shared
choice test. Run the relevant courses at scale factors 1, 1.25 and 2 and with
available style overrides. Treat offscreen style renders as layout evidence,
not native OS appearance acceptance. Fix any runtime failures before finishing
BPR-1659, then continue BPR-1660's pending inventory. No merge, push or release
is authorized by this handoff.
