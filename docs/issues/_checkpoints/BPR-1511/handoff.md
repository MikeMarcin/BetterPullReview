# BPR-1511 milestone checkpoint

Assignment: BPR-1511, isolated source/tracker worktree
`/Users/jaeger/.codex/worktrees/bpr1511-unread-badge/BetterPullReview`, branch
`bpr-1511-current-file-unread-badge`. Source baseline `d9d6d400d`.
Mandate revision: `owner-2026-10-14-current-file`, owner verbatim: "Badge for
unread comments should only show when there is one on the current file".
Strategy: preserved coordinator-implementer, non-delegating leaf. The adopted
single-milestone plan and binding preserve AC1–AC4 and distinct PR-wide behavior.

## Candidate and scope

Product candidate: `879d3c6b7632a4f4c7919697cdf53f755b254fab`.
Plan commit `ccb97cde6`; canonical claim `168201c88`.

The title comments toggle and Comments-tab pill count unread threads on the
semantic current file, independent of drawer filters. No selected file means
no badge, including a refresh gap that retains only a visual conversation
anchor. Next new comment and its count remain PR-wide. The existing unread
policy is unchanged. Counts share the existing forest/read-state cache and
single-thread read updates decrement the affected file without a review scan.
Badge descriptions are refreshed after generic drawer-action descriptions.

Changed product/documentation paths:

- `src/adapters/qt/include/bpr/qt/conversation_cards_view.hpp`
- `src/adapters/qt/src/conversation_cards_view.cpp`
- `src/adapters/qt/include/bpr/qt/workbench_window.hpp`
- `src/adapters/qt/src/workbench_window.cpp`
- `tests/adapters_qt/workbench_window_tests.cpp`
- `tests/adapters_qt/conversation_cards_view_tests.cpp`
- `tests/adapters_qt/CMakeLists.txt`
- `docs/ux/workbench-specification.md`

Frontend-design was read before UX edits. Design decision: retain the inherited
native palette/font/style, existing blue badge and tab geometry; scope the
information to this file. No new visual treatment or native appearance claim.

## Verification

- Configure: `cmake --preset dev -DCPM_SOURCE_CACHE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache -DBPR_DEPENDENCY_OFFLINE=ON` — pass.
- Build: `cmake --build --preset dev --target test_better_pull_review_qt_workbench_window_tests test_better_pull_review_qt_github_tests test_better_pull_review_qt_conversation_cards_view_tests -j 4` — pass.
- Final composed pass: `ctest --preset dev -R '^qt_current_file_unread_badge$|^qt_conversation_cards_view$|^qt_composed_github_scale_100$|^qt_workbench_conversation_reveal$|^architecture_boundaries$' --output-on-failure` — 5/5 pass, 34.25 seconds.
- The new course traverses file gesture → semantic dispatch → accepted selection;
  Mark read → read dispatch → one-thread read projection; full read-state and
  conversation refresh; empty selection; refresh gap; authority replacement.
- Shipped application: `cmake --build --preset dev --target app_better_pull_review -j 4` and `ctest --preset dev -R '^application_workbench_composition_contract$|^application_diff_workbench_public_path$' --output-on-failure` — build and 2/2 courses pass (17.32 s), exercising installed-Git/cache selection plus the cut-selection-wire negative control. Its full-window render was inspected.
- Card-cache comparison covers 32/4096 threads and verifies each file's count
  against complete state after incremental reads.
- Negative control: temporarily replace selected-file count with review-wide
  count; focused course fails at "the first file counts unread threads ...".
  Restored source and final passing run bind the real candidate.
- First focused run exposed generic action refresh overwriting the scoped
  accessible description; corrected update order and final pass verifies it.
  An initial test compile rejected reordered designated fields; corrected.
- `python3 _coding-standard/scripts/format.py --dry-run` on all six touched
  C++ files — pass with clang-format 22.1.8. `git diff --check` — pass.
- `python3 tests/scripts/verify_documentation_tests.py` — 67 tests pass.
- Offscreen full-window unread and clean-file renders inspected. Paths/digests,
  scale and native-appearance limits are in `render-evidence.json`. The PNGs
  remain ignored build evidence, not committed artifacts. Owner desktop unused.

Runtime logs: `/tmp/bpr1511-final-build.log`, `/tmp/bpr1511-final-tests.log`,
`/tmp/bpr1511-negative-control.log`, `/tmp/bpr1511-documentation-tests.log`,
`/tmp/bpr1511-app-build.log`, `/tmp/bpr1511-public-path.log`.

## Ownership and next action

This leaf is the only writer of this worktree. It preserved other agents'
scopes; BPR-1510 confirmed no shared-line overlap. MASK registration was
reviewed/applied during a coordinated exclusive slot; preflight is ready,
`host_load=not_verified`. No tracked deployment changes resulted.

All owned configure/build/test commands have finished. No child agents,
background writers or review server exist. The worktree/build/renders remain
for coordinator integration and inspection. No human criterion is required by
BPR-1511; parent RFC native/human obligations are unchanged.

Next action: coordinator authorizes existing note-only imports
`75dfc87c5..9b551b738` (48a1aa172, f47d44964, 9b551b738), then resume this
implementer for the documentation gate and canonical finish, then integration of only BPR-1511 plan,
claim, source and finish commits. Imported note-only counter-sync commits are
copies of existing main commits and should not be integrated again. No push
or main write was performed by this leaf.

The documentation gate and finish dry-run both refuse local NEXT1517 against
historical NEXT1520. High water tracks previous NEXT_ID values, so the existing
main imports through 9b551b738 suffice; no new allocation is needed. No further
import was made beyond the coordinator-authorized 75dfc87c5 boundary.
