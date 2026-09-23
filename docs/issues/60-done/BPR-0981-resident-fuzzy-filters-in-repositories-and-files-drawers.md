---
schema: "implement-rfc.card.v1"
id: "BPR-0981"
title: "Resident fuzzy filters with retained selection in the repositories and files drawers"
status: "Done"
rfcs: ["RFC-BPR-0025"]
depends_on: ["BPR-0596"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0025-planning-reconciliation","criteria":["AC-02","AC-09"]}
acceptance: [{"criterion":"AC-02 (whole): independent resident fuzzy filters find pull requests by repository, title and number and files by name or path, keep matching parent context, expose an honest empty result with a Clear filter action, send no provider request and leave the open pull request and selected file in place.","outcome":"pass","revision":"57439d16","evidence":{"kind":"command","detail":"ctest --preset dev --no-tests=error -R '^(qt_files_tree|qt_repositories_drawer|qt_composed_resident_drawer_filters|qt_workbench_focus_order_scale_(100|125|200))$' passes 6/6 at 57439d16 (build/rfc0025-bpr0981-ev-focused2.json). qt_files_tree: ordered case-folded subsequence, per-term match, folders kept in tree/flat. qt_repositories_drawer: repository/host/title/#number match, repository row kept, empty tree. Composed course through workbench_window: files 'cncon' keeps src/host, empty state names the filter, Clear filter restores rows and the selected file; repositories 'epoch'/'#73'/'zzz' with parent retention and empty notice; no new GitHub or workbench intent; rendered both empty states (composed-resident-drawer-filters-100.png, inspected). Negative controls: substring filter and dropped Applying guard each fail (build/rfc0025-bpr0981-negative.log). Producing commits fe41b77b, 861d75a1, 57439d16.","path":"docs/issues/30-in-progress/BPR-0981-resident-fuzzy-filters-in-repositories-and-files-drawers.md#verification--2026-09-22"}},{"criterion":"AC-09 (filters): both filters and the files order selector are declared, uniquely named, walkable focus stops in the composed window at 100, 125 and 200 percent.","outcome":"pass","revision":"57439d16","evidence":{"kind":"command","detail":"qt_workbench_focus_order_scale_100/125/200 (--focus-order) pass at 57439d16 with the exact device scale asserted: Filter pull requests then Clear pull requests filter precede the repositories tree; Files order, Filter files, Clear files filter precede the files tree; every declared stop focusable, named, described and unique; live focus chain walks the declared order. Dropping the repositories filter stop fails the 100 percent row (negative control). The full qt_workbench_window_scale_* suite stops earlier at BPR-0940's pre-existing epoch-row assertion, identical at e2f6eb1d.","path":"docs/issues/30-in-progress/BPR-0981-resident-fuzzy-filters-in-repositories-and-files-drawers.md#verification--2026-09-22"}},{"criterion":"Selection restoration by semantic identity in both trees produces no spurious open or select intent across filter, order and re-publication.","outcome":"pass","revision":"57439d16","evidence":{"kind":"command","detail":"qt_repositories_drawer: the chosen pull request is current again after filter, clear and queue re-publication with exactly one open; a window-opened pull request takes the current row without an open and the earlier row reopens on a fresh gesture; forget leaves nothing current. Composed course: the open pull request's row (#27) and the selected file are current again after both filters clear, no new intent; after the window opens another pull request the earlier tree row reopens (one open intent). Negative controls: dropped Applying guard and dropped follow hook each fail.","path":"docs/issues/30-in-progress/BPR-0981-resident-fuzzy-filters-in-repositories-and-files-drawers.md#verification--2026-09-22"}}]
---

# Resident fuzzy filters with retained selection in the repositories and files drawers

## Scope

The one product slice RFC-BPR-0025 still owns after the owner-locked
RFC-BPR-0028 direction superseded its layout and left browser-style tabs
unscheduled ([reconciliation](../../rfcs/plans/ux-redesign-pack-0028/rfc-0025-planning-reconciliation.md)).
It completes AC-02 and the filter half of AC-09 on the shipped drawers:

- One Qt-owned resident matcher shared by both trees. A needle matches when
  its characters, case-folded, appear in order in the candidate text; a
  contiguous substring is not required. The matcher decides nothing and
  performs no effect: the filter is presentation over the projections the
  trees already hold, as [[BPR-0658]] placed it, so it stays in the Qt
  adapter rather than the core or the host.
- The files drawer's existing `filesFilter` moves from literal substring to
  the matcher over the repository-relative path (which carries the file
  name); a matching file keeps its folders visible in every order.
- The repositories drawer gains its own `repositoriesFilter` above the tree,
  matching repository, pull request title and number; a matching pull
  request keeps its repository row and a matching repository keeps its
  rows, so parent context is never lost.
- Each drawer exposes an honest empty state when nothing matches, with a
  Clear filter action that restores the tree; neither filtering nor
  clearing closes or changes the open pull request or the selected file.
- Selection is restored by semantic identity: the files tree keeps the
  selected `workbench_file_id` across filter and rebuild (already so), and
  the repositories tree keeps the current pull request ref across filter
  and queue re-publication without dispatching a second open intent.
- Both filters and the files order selector join the window's declared
  focus order with unique accessible names and descriptions.

Owned surfaces: `src/adapters/qt` files tree, repositories drawer, workbench
window composition and focus order; `tests/adapters_qt` files tree,
repositories drawer, composed GitHub and workbench window courses;
`docs/ux/workbench-specification.md`. No core, host, provider, storage or
live GitHub change.

## Acceptance

- AC-02 (whole): independent resident fuzzy filters find pull requests by
  repository, title and number and files by name or path, keep matching
  parent context, expose an honest empty result with a Clear filter action,
  send no provider request and leave the open pull request and selected
  file in place.
- AC-09 (filters): both filters and the files order selector are declared,
  uniquely named, walkable focus stops in the composed window at 100, 125
  and 200 percent.
- Selection restoration by semantic identity in both trees produces no
  spurious open or select intent across filter, order and re-publication.

## Planned direct checks

- `qt_files_tree`: subsequence matching, case folding, ancestor retention in
  tree/flat/changed-first, empty state and clear, selected file retained.
- `qt_repositories_drawer`: repository/title/number matching, parent
  retention, empty state and clear, current pull request retained across
  filter and re-apply with exactly one open dispatch.
- `qt_composed_github_scale_100`: the public gesture path through the
  composed window with a published queue and pull request: type in both
  filters, read the trees, clear from the empty state, and assert the intent
  vector stays empty and the open pull request and selected file are unchanged.
- `qt_workbench_window_scale_100`, `_125`, `_200`: the declared focus order
  includes both filters and the order selector with unique accessible names.
- Touched-file format check and the documentation gate.

## Evidence

At planning head `9097723a` the files filter was a literal case-insensitive
substring over the path with no empty state beyond the line edit's clear
button, and the repositories tree had no text filter; neither filter was in
the declared focus order. The verified result is recorded under
[Verification — 2026-09-22](#verification--2026-09-22) below.

## Checkpoint — 2026-09-22, owner stop condition

Run `rfc-0018-0026-closeout-20260920`, segment `rfc-0025-claude`, milestone
`close-rfc-0025` (bound `build/rfc0025-claude-milestone-checkpoint-close.json`,
definition `d1cc095c…`, obligations BPR-0981 and BPR-0597). The owner's stop
condition arrived after the product, test and specification edits were
written and **before the first build**. They are committed as `fe41b77b`
so nothing is lost; that commit is **unbuilt, untested and unformatted**, and
nothing in it is claimed to pass.

What `fe41b77b` contains: `src/adapters/qt/include/bpr/qt/resident_filter.hpp`
and `src/resident_filter.cpp` (space-separated terms, ordered case-folded
subsequence match; `filter_empty_notice` with a named Clear filter button),
registered in `src/adapters/qt/CMakeLists.txt`; `files_tree.cpp` uses the
matcher; `repositories_drawer.hpp/.cpp` gain `set_filter`, a `rebuild` over
retained items and saved repositories (repository or host match keeps every
row beneath, title or `#number` match keeps the repository row; the rail
count stays the queue's), and the view keeps `Current` and restores it under
an `Applying` guard without a second open; `workbench_window.hpp/.cpp` add
the `repositoriesFilter` surface, both `filter_empty_notice` widgets,
`refresh_filter_notices()`, accessors and the declared focus stops; tests in
`files_tree_tests.cpp`, `repositories_drawer_tests.cpp`,
`composed_github_tests.cpp` (`test_drawer_filters_are_resident_and_keep_the_open_views`)
and `workbench_window_tests.cpp` (focus-order names); the File drawer
section of `docs/ux/workbench-specification.md`.

Next action for the resuming owner, in order: build
`test_better_pull_review_qt_files_tree_tests`,
`test_better_pull_review_qt_repositories_drawer_tests`,
`test_better_pull_review_qt_github_tests` and
`test_better_pull_review_qt_workbench_window_tests` with `cmake --build
--preset dev`; run `ctest --preset dev -R
'^(qt_files_tree|qt_repositories_drawer|qt_composed_github_scale_100|qt_workbench_window_scale_(100|125|200))$'`;
fix what fails (likely spots: the composed test's `choose_modality(review)`
visibility assumptions use `isHidden()` deliberately; `QStringView::split`
with `u' '`; the focus-order walk now includes the two Clear filter buttons
while hidden); run `coding-format-check` on the touched C++; then the
documentation gate, board validate, and `finish BPR-0981` with the exact
selectors and elapsed times before BPR-0597 starts. No attended verdict is
required by this slice; none was prepared.

No task-owned process is running. No push, merge to `main`, live provider
mutation or credential was used.

## Verification — 2026-09-22

Fresh `rfc-implementer` (coordinator-implementer, run
`rfc-0018-0026-closeout-20260920`, milestone `close-rfc-0025`) in
`/Users/jaeger/.codex/worktrees/0152/BetterPullReview` on
`codex/rfc-0018-0026-closeout`. Producing commits: `fe41b77b` (checkpoint),
`861d75a1` (build, verify and correct), `57439d16` (rendered empty states).

**Build.** `fe41b77b` compiled unchanged with the `dev` preset; none of the
three flagged spots failed (`isHidden()` visibility, `QStringView::split` with
`u' '`, hidden Clear filter stops in the focus walk). `cmake --build --preset
dev` of the whole tree passes at `861d75a1` (462 steps, no warnings; the
evidence rerun reports `ninja: no work to do`).

**Correction.** The repositories tree now keeps the window's *open* pull
request current, not merely the last row chosen in the tree: the window calls
`follow_open_pull_request` from `apply_github_pull_request` and
`forget_open_pull_request` from `clear_github_pull_request`. Without this, a
row chosen earlier was restored as current after the window opened another
pull request from Home or a URL, and clicking it again was swallowed as
already current. A tree-chosen row keeps its exact host and account; a
window-opened one is found by repository and number.

**Selectors.** The full `qt_composed_github_scale_100` and
`qt_workbench_window_scale_100/125/200` runs stop at assertions
[[BPR-0940]] owns (`with GitHub CLI chosen the card speaks for it…`, after
this course; `the epoch row is the first row of the client area…`, before the
focus-order course). Both reproduce identically at the pre-slice head
`e2f6eb1d` with this slice's sources reverted in place. As [[BPR-0976]] did,
explicit selectors run the new courses directly:
`qt_composed_resident_drawer_filters` (`--resident-drawer-filters`) and
`qt_workbench_focus_order_scale_{100,125,200}` (`--focus-order`, which now
asserts its exact device scale).

**Focused result at `57439d16`:**
`ctest --preset dev --output-on-failure --no-tests=error -R
'^(qt_files_tree|qt_repositories_drawer|qt_composed_resident_drawer_filters|qt_workbench_focus_order_scale_(100|125|200))$'`
passes 6/6 in 1.25 s (composed 0.26 s, files 0.16 s, repositories 0.16 s,
focus 0.23/0.21/0.22 s). Asserted: ordered case-folded subsequence and
per-term matching; folders kept in tree, flat and changed-first; repository,
host, title and `#number` matching with the repository row kept; honest
empty state naming the filter, with Clear filter restoring the rows; the
selected file and the open pull request's row current again after filter,
clear and queue re-publication with exactly one open; a tree row reopens
after the window opened another pull request; no new GitHub or workbench
intent through the composed filter and clear gestures; both
filters, both Clear filter buttons and the order selector declared, uniquely
named and walked ahead of their trees at 100, 125 and 200 percent.

**Negative controls** (`build/rfc0025-bpr0981-negative.log`, each file
restored and hash-checked): dropping the `Applying` guard fails
`qt_repositories_drawer` ("…without a second open") and the composed no-effect
assertion; a literal substring files filter fails `qt_files_tree` and the
composed course; dropping the window's follow hook fails the composed "open
pull request's row is current"; dropping the repositories filter stop fails
the 100 percent focus-order row.

**Rendered state.** `composed-resident-drawer-filters-100.png` in the build
tree shows both empty states in the composed window: "No pull requests match
“zzz”." and "No files match “nothing like this”." with Clear filter beside
each, the order selector beside the files filter, nothing clipped. Inspected
by the implementer; no attended verdict is required by this slice and none is
claimed.

**Hygiene.** clang-format passes on the slice's C++ lines
(`workbench_window.hpp:895` is misformatted at baseline and untouched).
clang-tidy `--warnings-as-errors` passes on `resident_filter.cpp`,
`repositories_drawer.cpp`, `files_tree.cpp` and the two small test TUs; the
large TUs report only baseline findings outside the slice's lines
(`dif_runtime.hpp` pragma once, `home_view_state.hpp` signed bitwise,
`workbench_window_tests.cpp:8143` from `4bbecb48`). `verify_documentation.py`
is `DOCUMENTATION_OK`; this worktree lacks the MASK `.agents` link, so the
gate ran with `PYTHONPATH` set to the installed implement-rfc scripts.

**Integration pass.** One full `ctest --preset dev -j 8` at `861d75a1`:
381/422 pass. All 41 failures reproduce at the pre-slice head `e2f6eb1d`
(separate detached worktree, full build) at identical failure points, timing
aside; none names this slice. They are recorded on [[BPR-0597]].

No core, host, provider, storage or live GitHub change; no credential, push
or merge to `main`.
