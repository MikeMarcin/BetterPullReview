# BPR-1666 verified milestone handoff

Assignment `/root/thread_scroll_fix`; mandate `thread-scroll-blocker-r1`, including
owner's2026-10-19 native-overlay clarification. Plan `bpr-1666-thread-scroll`,
milestone `repair-thread-scroll`; AC1–AC4 and both exits accepted technically.
Exclusive source/build root `/Users/jaeger/.codex/worktrees/thread-scroll-fix/BetterPullReview`,
branch `codex/thread-scroll-fix`. Baseline `b433d515f`; tested candidate
`8964ac489a8ed6ad5021a80f69f86d92fcdf3abd`. Product fix `3a6d1acbd`; final-line test refinement `8964ac489`.

## Cause and repair

`897df7ef0` clipped split-source viewports above every visible horizontal bar,
including Qt-native overlays. That reserved an18px band, changed measured
viewport height/the shared43% rail, and allowed geometry publication to reveal
the retained conversation line again, undoing user scrolling. The diff source
is identical in v0.3.0 and the Qt6.12 baseline; this is not attributed to the
upgrade. Qt6.12's official `src/plugins/styles/mac/qmacstyle_mac.mm` maps
`PM_ScrollView_ScrollBarOverlap` to the bar extent when the native preferred
scroller style is overlay. The repair uses that native metric to preserve the
full source viewport; non-overlapping bars retain their existing band.

Old-product negative controls under normal isolated CTest both fail: source
viewport y1..583 vs overlay y584..601; production Overview unread status ->
Only new Threads -> row double-click -> long-comment wheel, with overlay
show/hide between turns, snaps shared position7842 back to3682, total9 rebuilds.
Plain wheel/idle without the visibility transition initially passed. The
regression explicitly supplies that native visibility transition.

Candidate overlay is y1..601 with bar y584..601. Every scroll/visibility turn
retains its exact delayed position, total rebuilds stays4, source-end overflow
reaches the final line of the entire100-paragraph comment, and idle remains
stable at100/125/200%. The complete long-comment end captures and the full-window
Git/cache composition capture were inspected; hashes and paths are external
`/tmp/bpr-qt612-run/scroll-render-manifest.json`.

## Verification

macOS arm64, Qt6.12.0, offscreen, serial GUI tests; clang-format/tidy22.1.8.

- Initial candidate2/2: diff workbench100 + route100,25.81s.
- Remaining geometry/window matrix10/10,227.85s: diff125/200, thread moves100/125/200,
  route125/200, workbench window100/125/200.
- Final integration11/11,106.82s: architecture, composed GitHub100, inline drawer100,
  long thread100/125/200, cards, strengthened final-line route100/125/200,
  exact application diff public path (6.76s; real Git/cache, conversation anchor,
  full-window render).20 distinct rows,23 executions; product inputs unchanged
  by the test-only refinement, so unchanged passing evidence is reused.
- clang-format22 dry-run on all3 touched TUs; strict clang-tidy22 on those3 TUs;
  DOCUMENTATION_OK;67/67 documentation-verifier tests; git diff --check.

Commands and logs:
`cmake --preset dev -DCPM_SOURCE_CACHE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache -DBPR_QT_ROOT=/tmp/bpr-qt612-run/qt/6.12.0/macos -DCMAKE_PREFIX_PATH=/tmp/bpr-qt612-run/qt/6.12.0/macos -DBPR_DEPENDENCY_OFFLINE=ON`.
Focused build targets: `test_better_pull_review_qt_workbench_window_tests`,
`test_better_pull_review_qt_diff_workbench_tests`,
`test_better_pull_review_qt_github_tests`,
`test_better_pull_review_qt_conversation_cards_view_tests`, `app_better_pull_review`.
CTest selectors are preserved in `/tmp/bpr-qt612-run/scroll-*-ctest.log` and
`scroll-geometry-matrix.log`; detailed child output in `scroll-*-lasttest.log`.
Coding/documentation logs are `scroll-format-final.log`, `scroll-tidy.log`,
`scroll-documentation-final.log`, `scroll-documentation-tests.log` there.

## Scope and upkeep

Frontend-design was loaded first, plus installed author-rfc/implement-rfc,
shared engineering contract and coordinator-implementer recipe. No new RFC was
needed for this bounded regression. MASK registration/preflight ready_pinned;
no upgrade. Source policy, provider/credential/publication/persistence ownership,
version and package behavior are unchanged. Workbench specification documents
the native source-overlay extent.

Main was clean at initial inspection; only counter/receipt reservation commit
`84e06ec28` was made there under explicit authority, adopted as `4fb7dcde0` here.
Plan `a2ebac53d`, claim `edec63c78`. No source integration into main, push, live
GitHub write, release bump/publication, personal-app/desktop operation or agent
spawn. The Qt-upgrade owner's tree/build were not modified. Builds were held
and later quiesced as agreed while its search-performance row ran.

Target-only checkpoint bind encounters the same unrelated shared-action-buttons
plan missing-boundary refusal documented on BPR-1665. Explicit plan bind passed;
checkpoint uses that binding and the canonical repository-state helper. The
final explicit binding remains byte-for-byte unchanged after execution records.

## Ownership, review and continuation

All task-owned configure/build/test/tidy commands have completed. Former build
PGID72266 exited; no descendant remains. No review server, pending human
criterion, native/independent acceptance gate, or candidate protection. Retain
this worktree/build and external evidence for coordinator integration; no active
source writer remains after handoff.

Next: coordinator integrates this committed milestone with the completed Qt6.12
upgrade, then its fresh release implementer handles v0.3.1 qualification and
publication. No further broad tests are requested by this milestone.
Offscreen forced-style evidence proves geometry/interaction, not native visual
skin or physical input. Native Windows/Linux and package/release qualification
are subsequent release work; this milestone makes no such claim.
