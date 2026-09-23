# Overview folder collapse and final reconciliation

## Assignment and mandate

Run `rfc0037-run-20260918`, segment `overview-folder-collapse-20260920`,
mandate revision `1`; owner `/root/rfc0037_final_reconcile` under the selected
coordinator-implementer recipe. The coordinator confirmed that the existing
mandate to implement RFC-BPR-0037, revise stale planning and land after closeout
includes the newer owner request BPR-0934 (attended decision 441, 2026-09-20).

The source/tracker root is `/Users/jaeger/.codex/worktrees/714d/BetterPullReview`,
branch `codex/rfc0037-overview-folders`, starting revision
`a578f8ec07fb1dd1eb98192e61ab8473152b023b`. The immutable RFC registration
baseline remains `9bd2002b859ce4c5599d86dcc51771aa365fa90b`.

## Plan reconciliation

Target discovery found the adopted four-milestone plan. All four are accepted
by their existing returns, ending with `milestone-return-rfc0037-dispatch-004.json`.
The current board contains twelve Done cards and BPR-0934 in Backlog, with no
unmet dependencies. The newer card was missing from the adopted plan.

`milestone_plan.py check-amendment` refused an additive proposal with
`amendment must preserve every obligation; reconcile changed requirements separately`.
That helper checks same-scope refinements only. The owner request and coordinator
assignment explicitly authorize this added requirement, so it is reconciled as
AC-10 and an appended `overview-folder-collapse` milestone. A structural comparison
confirmed the original four milestone definitions, work selectors and exit texts
are byte-for-byte equivalent as JSON; no original obligation or pending acceptance
was removed. Original intake/proposal/provenance records are unchanged.

## Diagnosis and implementation

The original Overview explicitly disabled expansion, used flat full-folder groups,
and called `expandAll()` on every presentation. The new rendered public regression
failed against claim revision `0742cc13` at the native-disclosure assertion. This
established the shortest public-path failure before the product edit.

Product candidate `a88c0d19e00cbf035bafd282c2701c862ddac623` provides a complete
nested hierarchy, including implicit ancestor folders, with native disclosure in
the file column. Surviving folder/file nodes retain stable indexes: metadata-only
refresh updates rows in place and structural refresh uses insert/move/remove
signals, never a model reset. New folders start expanded; surviving folders keep
their Qt-local state. Clearing the owning authority removes the old nodes and
selection. Collapsing a folder containing the current item puts focus on that
visible ancestor without dispatching file activation. The core, host, provider,
comparison and publication boundaries are unchanged.

Product ownership is limited to the Overview header/source, its focused Qt test,
the existing composed setup/public-path test and the workbench UX contract. The
code-simplifier review retained the direct tree reconciliation and existing style;
no unrelated refactor was necessary. Self-review checked retained-pointer/index
lifetime, parent/row updates, move ordering, ancestor counts, foreign-index guards,
new-subtree expansion and authority clearing against the focused regressions.

## Exact-candidate verification

Build and test output is private to this checkout. Configuration used:

```sh
cmake --preset dev -DBPR_QT_PROVIDER=OFF -DBPR_QT_ROOT=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache/bprqt/6.10.3/6.10.3/macos -DCPM_SOURCE_CACHE=/Users/jaeger/.codex/worktrees/714d/BetterPullReview/build/.cpm-cache
cmake --build --preset dev --parallel 4 --target test_better_pull_review_qt_pull_request_overview_page_tests test_setup_destination_public_path
BPR_SETUP_ARTIFACT_ROOT=/Users/jaeger/.codex/worktrees/714d/BetterPullReview/build/rfc0037-overview-artifacts ctest --preset dev -R '^(qt_pull_request_overview_page|application_setup_destination_public_path)$' --output-on-failure
```

Despite the optional root hint, the actual selected package is Homebrew
`/opt/homebrew/lib/cmake/Qt6`, Qt **6.11.2**, offscreen on macOS arm64 at DPR 1.
The main checkout's build output was not modified. Configuration and focused
build passed. The final batch after restoring the exact committed candidate
passed **2/2 in 41.81 seconds**: Overview 2.20 seconds and composed public path
39.58 seconds. Earlier exact-candidate runs also passed (1.98 and 38.48 seconds).

The focused test covers implicit/nested ancestry, native mouse and keyboard input,
accessible name and expandable/expanded state, descendant visibility, retained
persistent indexes/order/focus, metadata refresh, insertion/removal/reordering,
nested collapse retention and clearing on authority invalidation. Qt's native
accessible tree cell reports `expanded`, not a separate `collapsed` bit; the
assertion follows that implementation rather than inventing a state requirement.

The composed recorded-fixture course reaches the actual loaded-PR Overview,
collapses/reopens its folder without choosing a file or source comparison, then
continues through the pre-existing update/source/Discuss/local-draft route with
zero publication effects. Agent-inspected full-window captures
`build/rfc0037-overview-artifacts/overview-folders-collapsed.png` and
`build/rfc0037-overview-artifacts/overview-folders-expanded.png` show the native
disclosure and correct file visibility. Captures are disposable build artifacts,
not committed assets or human acceptance.

## Broader checks and baseline limitations

The integration-focused batch deliberately included `qt_composed_github_scale_100`;
it failed at the existing CLI sign-in-card visibility/default-button assertion.
An additional `qt_workbench_window_scale_100` run failed at the existing full-width
epoch-row layout assertion. Neither failure is counted as a pass.

Both were reproduced by temporarily applying the two owned Overview production
files from `0742cc13`, rebuilding both unchanged suites and running them together:
the sign-in assertion failed in 3.31 seconds and epoch-row assertion in 2.45
seconds. The remainder of the branch differs from that baseline only in new tests
and documentation. The exact `a88c0d19` files were then restored, verified with an
empty product diff, rebuilt, and the 2/2 passing batch above rerun. This establishes
both broader failures as baseline behavior under this Qt/offscreen environment;
it does not claim those suites are green or diagnose their separate fixes.

```sh
cmake --build --preset dev --parallel 4 --target test_better_pull_review_qt_github_tests test_better_pull_review_qt_workbench_window_tests
ctest --preset dev -R '^(qt_composed_github_scale_100|qt_workbench_window_scale_100)$' --output-on-failure
python3 _coding-standard/scripts/format_check.py --root . src/adapters/qt/include/bpr/qt/pull_request_overview_page.hpp src/adapters/qt/src/pull_request_overview_page.cpp tests/adapters_qt/pull_request_overview_page_tests.cpp tests/end_to_end/setup_destination_public_path_tests.cpp
python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --warnings-as-errors --paths src/adapters/qt/src/pull_request_overview_page.cpp tests/adapters_qt/pull_request_overview_page_tests.cpp tests/end_to_end/setup_destination_public_path_tests.cpp
```

Formatting passed all four touched C++ files with clang-format 23.1.1. Full-file
tidy passed the changed product and focused Qt test translation units. The composed
test translation unit has three inherited diagnostics, all present at `0742cc13`:
signed bitwise enum initialization in `home_view_state.hpp:35`, the existing
`std::stop_token stop` callback argument, and the existing signed `value >> shift`.
The end-to-end tidy invocation therefore failed; those unrelated baseline lines
were not changed. Initial compile/assertion mistakes during development were fixed
before the committed candidate and its final passing checks.

## Documentation, acceptance and current gates

The raw documentation command could not import `board_model` because this worktree
has no `.agents` deployment. A command-local import path to the installed shared
skill provides the exact required module without changing project infrastructure:

```sh
PYTHONPATH=/Users/jaeger/.agents/skills/implement-rfc/scripts python3 scripts/verify_documentation.py --root .
PYTHONPATH=/Users/jaeger/.agents/skills/implement-rfc/scripts python3 tests/scripts/verify_documentation_tests.py
python3 /Users/jaeger/.agents/skills/implement-rfc/scripts/issue_board.py --repo-root . validate --rfc RFC-BPR-0037 --json
git diff --check
```

Documentation verification returned `DOCUMENTATION_OK`; all 64 verifier tests
passed. Board validation was valid with 957 cards and no errors. Closeout commit
`f6103a7dc874caa0483b9efd93c7c1d14153410b` moved BPR-0934 to Done with four
direct, revision-bound records in `acceptance-BPR-0934.json`; the scoped queue is
now 13/13 Done with no next card. No issue ID was allocated or reused. The
original four milestone definitions and AC-01 through AC-09 acceptance records
are unchanged; AC-10 is additive and separately accepted. The final checkpoint
reconciles the card's expected source fingerprint change caused by recording
evidence and Done acceptance, with no change to its four requirements.

The previous implementation candidate `e4096e05` is already in main history.
Formal status remains **Proposed**: RFC-0008, 0013, 0015 and 0028 are Proposed,
and 0029 is Accepted. None is silently closed by this milestone. The separate
main checkout is actively owned by BPR-0954 with uncommitted release-maintenance
changes; it is neither changed nor paused. The new branch is not landed in main.

The original attended macOS decisions remain historical acceptance and do not
certify the new folder interaction. BPR-0934 requires the rendered automated
course, not another owner verdict. No review server is running or handed off.
Windows/Linux, live GitHub/GHES, literal `gh`, private-repository behavior and
successful in-process Refresh recovery remain unclaimed as in the prior closeout.

## Ownership, durable handoff and next action

Plan preparation `f91b73ff` and claim `0742cc13` precede product candidate
`a88c0d19`. Closeout commits contain only the named RFC/index/card/plan evidence
paths; the original accepted records, other worktrees and main writer are preserved.
All task-owned builds and tests are finished before handoff. There are no task
servers, background writers or attended protections newly created by this slice.
The product candidate remains protected by `codex/rfc0037-overview-folders`.

`checkpoint-overview-folder-collapse-20260920.json` and
`observation-overview-folder-collapse-20260920.json` bind closeout candidate
`f6103a7d` (product unchanged from `a88c0d19`), the five-milestone plan, completed
card, exact candidate repository state,
accepted evidence and stop decision. The whole RFC/landing request remains blocked
by canonical status eligibility and the active main checkout, despite complete
BPR-0934 engineering. Next: the coordinator reports these explicit gates. Once
the dependency statuses permit closeout and main's owner has completed its work,
reconcile the branch against the then-current main and land without discarding
concurrent changes. Do not expand this milestone into dependency implementation.

At `f6103a7d`, the checkpoint helper returned `action: stop` for this reconciled
observation; its `engineering_acceptance: not-evaluated` correctly leaves the
acceptance judgment with this implementer. The later handoff commit only persists
these checkpoint/observation/evidence records. It does not change product or test
sources, card requirements, acceptance, plan definitions or dependency status.
Replaying the frozen observation at a later repository revision requires an
explicit current-state reconciliation, not silently treating its revision as HEAD.

Final post-Done documentation and board checks passed: `DOCUMENTATION_OK`, 64/64
verifier tests, 957-card validation with no errors, valid milestone-plan references
and `git diff --check`. All invoked command sessions completed. The disposable
additive-plan proposal was removed; ignored build output and the two inspection
captures remain available for local evidence and were not committed.

## Main landing (2026-09-23)

### Assignment and recovery

On 2026-09-23 the owner asked to "take up codex/rfc0037-overview-folders
branch and see if we can land that on main". The coordinator dispatched a
coordinator-implementer leaf on `main`. That first leaf built main at
`392751fc` and ran the 13-test baseline batch. It then started
`git merge --no-ff --no-commit`, resolved the conflict, and built and reran
the batch on the merged tree. It stopped before committing. A fresh leaf
recovered that work from its transcript and scratch logs, confirmed that the
staged index held exactly the branch-owned paths, and completed the landing.
A concurrent docs-only writer (RFC-BPR-0024 amendment r4 and BPR-1019) shared
main throughout. Every commit used explicit paths, and neither writer staged
the other's files.

### Assessment

The branch was 5 commits ahead of `392751fc` and 245 behind it, from base
`a578f8ec`. It contains plan `f91b73ff`, claim `0742cc13`, product `a88c0d19`,
Done acceptance `f6103a7d` and handoff `b0001610`. BPR-0934 was complete and
Done on the branch but still in Backlog on main. Main had not touched the
Overview page sources, its focused test, this plan or the RFC body since the
base. So nothing in the branch was superseded. Main did change the composed
setup test (BPR-0987) and the workbench UX specification, and both merged
automatically. Both gates the branch recorded have cleared: BPR-0954 no longer
owns the main checkout, and RFC-BPR-0037's dependency status only limits the
RFC's own status, not landing an accepted slice. The decision was to land the
whole branch with a merge commit.

### Merge

Merge `0ddf929b` preserves all five branch commits. The one conflict was the
RFC-BPR-0037 row in `docs/rfcs/README.md`. The resolution keeps main's newer
dependency statuses and adds the branch's AC-10 note. The branch touched
neither `NEXT_ID` nor `_reservations`, so no ID was duplicated or renumbered.
`verify_documentation.py` reported no digest drift, so nothing was resealed.

### Masked regression and correction

The baseline and merged-tree batches both failed seven tests, with identical
assertions. The full `qt_workbench_window` run stops at the BPR-0940
epoch-row assertion, so its later Overview courses never ran on either side.
Run directly through the new `--overview` entry, the merged tree failed
`test_identical_overview_skips_rebuild_but_restores_cleared_files` at "initial
overview projects both real file models". That course existed at the base, from
`4a2be1b1`, and counted the page model's `modelReset`, which the retained tree
never emits. Temporarily restoring main's two Overview production files made the
same entry pass, which places the regression on the branch. The merged files
were restored afterwards and matched `HEAD`.

Test-only correction `b6e14081` counts reset, rows inserted, removed or moved,
and layout changed as page rebuilds. Equal overviews must emit none of them;
reopening after a clear or an authority clear must emit one. The commit also
registers `qt_workbench_overview`, which runs the window's three Overview
courses directly. Product code remains `a88c0d19`.

### Verification at `b6e14081`

```sh
cmake --build --preset dev
ctest --preset dev -R '^(test_network_isolation_structure_contract|test_network_isolation_manifest|application_pull_request_overview|qt_composed_github_scale_(100|125|200)|qt_pull_request_overview_page|qt_shared_snapshot_navigation|qt_workbench_focus_order_scale_(100|125|200)|qt_workbench_home_discovery_scale_(100|125|200)|qt_workbench_conversation_reveal|qt_workbench_overview|qt_native_window_geometry|qt_workbench_window_scale_(100|125|200)|qt_composed_markdown_(1_0|1_25|2_0)|application_composition_contract|application_workbench_composition_contract|application_overview_since_review_contract|application_setup_destination_public_path|application_profile_multi_window_public_path)$' --output-on-failure
python3 _coding-standard/scripts/format_check.py --root . tests/adapters_qt/workbench_window_tests.cpp
python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --warnings-as-errors --paths tests/adapters_qt/workbench_window_tests.cpp
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
```

The full dev build passed. The integration pass ran 28 tests, and 20 passed:
`qt_workbench_overview`, `qt_pull_request_overview_page`,
`application_setup_destination_public_path`, the Overview-since-review,
multi-window and workbench-composition contracts, the composed Markdown scales,
and every other direct entry in the workbench binary. The eight failures all
reproduce from main with unchanged assertions:

- `qt_composed_github_scale_*`: the BPR-0940 sign-in card.
- `qt_workbench_window_scale_*`: the BPR-0940 epoch row.
- `application_composition_contract`: the five BPR-0983 assertions.
- `test_network_isolation_manifest`: the BPR-0962 `release_update_native_https`
  row. It is the only finding reported, so the new registration is accepted.

clang-format passed. clang-tidy found three inherited diagnostics and none on
changed lines: `dif_runtime.hpp:1`, `home_view_state.hpp:35` and the
`wait_for_frame` naming at `4bbecb48`.

The documentation gate and its 64 verifier tests passed, both on the merged
tree and after these records. Board validation passed. Evidence remains offscreen
Qt 6.11.2 on macOS arm64. It asserts no human, native Windows/Linux or live
GitHub result.

### Checkpoint and retirement

`milestone_checkpoint.py bind` refuses the milestone because it is already
accepted. Replaying the frozen branch observation would bind a different
worktree and revision. So this section is the landing reconciliation, and the
branch checkpoint and observation records stay unchanged. Every task-owned
build and test has finished, and no server or background writer remains.
`codex/rfc0037-overview-folders` is fully contained in main, so its branch
and the `.codex/worktrees/714d` worktree can be retired by whoever owns them.
Next: a status-only RFC-BPR-0037 reconciliation once RFC-BPR-0028 and
RFC-BPR-0029 are `Auto-Verified`.
