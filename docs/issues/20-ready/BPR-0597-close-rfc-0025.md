---
schema: "implement-rfc.card.v1"
id: "BPR-0597"
title: "Verify and close RFC-BPR-0025"
status: "Ready"
rfcs: ["RFC-BPR-0025"]
depends_on: ["BPR-0596","BPR-0981","BPR-0985"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0025"]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"planning_reconciliation":"docs/rfcs/plans/ux-redesign-pack-0028/rfc-0025-planning-reconciliation.md"}
---

# Verify and close RFC-BPR-0025

## Objective

Reconcile the complete accepted behavior in [RFC-BPR-0025](../../rfcs/RFC-BPR-0025-review-workspace-navigation-browser-tabs.md)
against direct evidence, all linked issue cards, and the final public path.
This closeout depends on the entry card now and must acquire all subsequently
allocated required slice dependencies before it can become startable.

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 3 of 4.
Planned cards: BPR-0596, BPR-0597, BPR-0981. Allocate additional implementation cards only
when the later bounded-slice planning pass needs them, and update this list and
closeout dependencies before implementation. The RFC index remains the order
authority; this requested subset waives no intervening eligibility/closeout gate.

**Design accepted; registration only; implementation requires a later request.**
This unstarted backlog card records the 2026-09-04 registration, not an execution
grant or completed product work. The immutable base is the pre-registration
commit for this approved workstream; registration commits after it are planning
history, not evidence of interrupted product implementation.

## Closeout checklist

Planning reconciliation on 2026-09-22 ([record](../../rfcs/plans/ux-redesign-pack-0028/rfc-0025-planning-reconciliation.md))
adds the one required slice BPR-0981 to the planned cards and dependencies.
The owner-locked RFC-BPR-0028 direction supersedes this RFC's layout and
focus-mode chrome where they conflict and leaves browser-style file tabs
unscheduled; each row below names the surviving check or the superseding
decision. A superseded row is recorded as such at closeout, never as passed.

| Criterion | Executed direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: The shipped review surface has independently collapsible Reviews, Files, and Conversations drawers, compact center chrome, and reversible focus mode with window-specific restoration. | `application_profile_window_layout_public_path`; `qt_workbench_window_scale_100/125/200` (integration run at `861d75a1`, product unchanged to `2fbcc8cb`) | Surviving part pass: the layout course passes 24.63 s (per-window drawer layout and geometry across Close, Quit and restart). In `qt_workbench_window_scale_100/125/200` the drawer-toggle, collapse-returns-space and strip-modality courses run and pass before each run stops at [[BPR-0940]]'s pre-existing epoch-row assertion (identical at `e2f6eb1d`); the rails course after it is not reached at this head. Reviews drawer, compact centre and focus mode superseded by RFC-BPR-0028 (owner lock 2026-09-06; accepts on BPR-0646/0651/0662/0721/0724/0727): superseded, not passed | BPR-0977 `3cf05584`; rerun `861d75a1` |
| AC-02: Independent fuzzy filters find PRs by repository/title/number and files by filename/path, preserve matching context, and expose honest empty results without provider effects or lost active views. | [[BPR-0981]]: `qt_files_tree`, `qt_repositories_drawer`, `qt_composed_resident_drawer_filters` (composed gesture path), `qt_workbench_focus_order_scale_100/125/200` | Pass: 6/6 at `57439d16` (`build/rfc0025-bpr0981-ev-focused2.json`); resident subsequence filters by repository, host, title, number, file name and path with parent context, honest empty state with Clear filter, no new GitHub or workbench intent, open pull request and selected file kept; negative controls fail as expected. BPR-0981 Done `2c4dfcd1` | `fe41b77b`, `861d75a1`, `57439d16` |
| AC-03: File clicks and previous/next file navigation reuse the active tab and preserve its comparison; Cmd-click, Ctrl-click equivalents, and Open in New Tab create a distinct background view. | `core_workbench_interaction`; `application_workbench_host` `test_file_navigation_keeps_the_accepted_comparison` | Single view pass at `2fbcc8cb` (`build/rfc0025-closeout-ev-ac03.json`): a file click, next file and previous file each look up the file they reached under the exact comparison the epoch host accepted, and each publishes in the one view. Cmd/Ctrl-click and Open in New Tab superseded/unscheduled by RFC-BPR-0028: not implemented, not passed | `2fbcc8cb` |
| AC-04: Duplicate Tab preserves the exact initial view while allowing independent epochs, positions, and selections even for the same file in multiple tabs. | None: Duplicate Tab is wholly tab-specific | Superseded/unscheduled by RFC-BPR-0028 browser-tab non-goal; not implemented; not passed | None |
| AC-05: Each window's PR retains its tab order and active view across PR switching and restart; closing tabs or PRs follows the specified neighbor/landing behavior without deleting private review work. | `application_profile_window_review_restoration_public_path` (BPR-0709/0710); `application_profile_window_restart_public_path`; `application_profile_multi_window_public_path`; restoration and ledger host, core and storage tests | **Fail, pre-existing; blocks closeout.** The restoration course fails at `2fbcc8cb` and at the pre-slice head `e2f6eb1d`. With the BPR-0795 correction (`2fbcc8cb`) seed, auth-home, auth-stale, unavailable-anchor/file/content, missing-grant/server, corrupt, future, failed-read and the anonymous and capability phases pass; `auth-reading-gesture/unchanged/home` fail “gesture precedes deferred reading restoration completion” and `auth-restore` fails “restart preserves exact anchors, commit OIDs, file and complete semantic identity”: comparison and file restore exactly, the semantic selection differs. auth-close and auth-verify are not reached. Passing at `861d75a1`: restart 8.62 s, multi-window 84.28 s, `application_window_review_restoration`, `core_window_review_restoration`, `sqlite_window_review_restoration_storage`, `application_review_ledger_composition_contract`. Tab order and active tab superseded | Last pass BPR-0709 `ce63c67b`; red at `e2f6eb1d`, `2fbcc8cb` |
| AC-06: New revisions, removed files, pruned epochs, and unavailable comparisons never silently retarget an exact view; named-comparison updates require explicit acceptance in the affected tab. | `core_epoch_policy`; source inspection of `refresh_epochs_intent`; restoration unavailable phases (BPR-0710) | **Partial fail, pre-existing; blocks closeout.** `core_epoch_policy` passes; at `2fbcc8cb` the only production dispatch of `refresh_epochs_intent` is the explicit Refresh action (`epoch_comparison_control.cpp:542`); unavailable-anchor, -file and -content keep the destination and sibling reading without substitution. `unavailable-semantic` fails “unavailable reading reports explicit failure” (destination kept, explicit report missing), identically at `e2f6eb1d`. Tab-scoped update indication superseded | Run `861d75a1`; inspection `2fbcc8cb` |
| AC-07: Late work cannot populate another profile, window, PR, tab, or comparison, and inactive tabs retain bounded lightweight state without eager full-diff loading. | `application_workbench_host`, `application_workbench_snapshot_observation`, `application_epoch_host`; RFC-0024 exact-authority courses | Surviving part pass: the three host tests pass at `861d75a1` (host rerun `2fbcc8cb`); profile, window, pull request and comparison exactness from [[BPR-0976]], [[BPR-0977]], [[BPR-0978]] (Done), with `application_profile_multi_window_public_path` and `application_profile_two_account_isolation_public_path` passing at `861d75a1`. Tab identity and bounded inactive tabs superseded, not passed | BPR-0976 `8d072d14`, BPR-0978 `6f8e44e4`; rerun `861d75a1` |
| AC-08: The existing diff mapping, linked scrolling, selected-line identity, tuned appearance, review-draft authority, and focused-review space behavior remain intact in the composed tabbed workbench. | `qt_diff_workbench_scale_100/125/200`, `application_diff_workbench_public_path`, `application_review_ledger_composition_contract`, `application_appearance_theme_customization_contract`, `qt_composed_github_scale_100` | Surviving part pass at `861d75a1`: diff workbench 100/125/200, the diff public path (13.26 s), ledger composition (37.90 s) and theme customization (26.92 s) pass. `qt_composed_github_scale_100` runs 32 of its 39 courses (loading, conversation, layout, minimum geometry, drawers, filters, read drawer, one-pane threads) before [[BPR-0940]]'s pre-existing card assertion, identical at `e2f6eb1d`; the seven unreached are server-card and sign-in courses. “Tabbed workbench” read as the shipped RFC-BPR-0028 composition | Rerun `861d75a1` |
| AC-09: Keyboard, focus, accessibility, long labels, narrow-window layouts, and 100–200% scaling keep tabs, filters, drawers, comparison controls, and review actions reachable. | `qt_workbench_focus_order_scale_100/125/200`, `qt_composed_resident_drawer_filters`, `qt_composed_github_scale_100/125/200` | Surviving part pass: focus-order rows at exact scale and the composed filter course pass at `57439d16`; at every scale the composed suite passes its minimum-geometry render and drawer-reachability courses before [[BPR-0940]]'s stop. The workbench-window drawer width floor course follows BPR-0940's epoch-row stop and is not reached at this head. Tab strip superseded, not passed | `861d75a1`, `57439d16` |

## Completion rules

- Replace planned checks with exact executed commands/courses, results, and
  producing commits as implementation completes; retain not-run rows honestly.
- Inspect all cards linked to this RFC, not just the initial planned pair.
  Every required card must be done and every canonical criterion satisfied.
- Exercise the selected public path and check critical changed-surface findings.
  Use focused evidence; do not infer a full matrix requirement from this card.
- Record target-native or attended evidence in the project's existing workflow.
  Park pending attended work and continue independent work; no approval or live
  GitHub authority comes from registration.
- Run the documentation structure gate and `git diff --check`, reconcile RFC
  and index status, and close only when canonical order and all dependencies
  permit Auto-Verified. No status changes are authorized by this registration.

## Resume

The owner activated the ordered workstream in the canonical index; RFC-0018
and RFC-0024 are Auto-Verified. [[BPR-0596]] recorded the 2026-09-22 planning
reconciliation: RFC-0028's superseding layout and unscheduled browser-style
file tabs are mapped per criterion above, and [[BPR-0981]] is the one
allocated slice. This closeout becomes startable when BPR-0981 is Done; it
then re-runs the surviving checks at the closeout head, records superseded
rows as superseded, and reconciles the RFC and index. This record claims no
tab implementation or acceptance.

Stopped 2026-09-22 under the owner's stop condition with [[BPR-0981]] In
Progress at `fe41b77b`, an unbuilt checkpoint. A fresh implementer resumed the
same day; the result follows.

## Closeout verification — 2026-09-22

Run `rfc-0018-0026-closeout-20260920`, milestone `close-rfc-0025`, fresh
non-delegating `rfc-implementer` (coordinator-implementer) in
`/Users/jaeger/.codex/worktrees/0152/BetterPullReview` on
`codex/rfc-0018-0026-closeout`. [[BPR-0981]] is Done at `2c4dfcd1`; this card
was made Ready at `50bfc54b` and claimed at `a6c278f3`. Closeout head
`2fbcc8cb`: product code is unchanged since `861d75a1`, and the only later
source changes are the two test courses named below.

**Integration pass.** One full `ctest --preset dev -j 8` at `861d75a1`
(1,381 s): 381 of 422 pass. All 41 failures reproduce at the pre-slice head
`e2f6eb1d`, built in a separate detached worktree and run with the same
parallelism, at identical failure points apart from timing
(`build/rfc0025-integration-full*.log`,
`build/rfc0025-baseline-e2f6eb1d-failed41*.log`). None names
BPR-0981 or this closeout's changes. By cause:

- *This worktree lacks the MASK `.agents` link:* `documentation_structure`,
  `documentation_issue_graph_contract`, `issue_id_allocation_contract`. The
  gate itself is `DOCUMENTATION_OK` with `PYTHONPATH` set to the installed
  implement-rfc scripts.
- *No network in the sandbox:* `dependency_modernization_contract` (pypi.org
  name resolution).
- *Tracked:* `test_network_isolation_manifest` ([[BPR-0962]]'s
  `release_update_native_https` row); `qt_composed_github_scale_100/125/200`
  and `qt_workbench_window_scale_100/125/200` ([[BPR-0940]]).
- *Untracked, pre-existing:* `msvc_secure_crt_contract` (`std::getenv` in the
  release-update tests); `qt_anonymous_public_preview_scale_100/125/200`
  ("GitHub.com's Connect, while it is not chosen, activates exactly its native
  HTTPS profile"); the application public-path family with core facts
  `presentation=0 overview=0` (`application_public_path`,
  `application_public_path_ghes_cli_unavailable`, four
  `application_device_authorization_*`, four
  `application_anonymous_public_preview_*`, `application_render`,
  `application_appearance_store_fallback_contract`,
  `application_workbench_composition_contract`,
  `application_uiforetw_selected_diff_contract`); `application_threaded_reply_contract`
  (40 s run budget); `application_composition_contract` and
  `application_review_round_trip_composition_contract` (`github_application.cpp`
  digest pin drifted); `application_composition_capabilities_*` (notification
  permission refused); four `application_large_review_*` (Mac footprint
  budget); `application_agent_finding_dif_context_contract`;
  `live_review_mutation_provider` (unscripted GraphQL request); and
  `application_profile_window_review_restoration_public_path` (below).

**The blocker: reading-position restoration.** The window review restoration
course that AC-05 and AC-06 re-run is red at the closeout head and at
`e2f6eb1d`; it last passed as BPR-0709's evidence at `ce63c67b`. Its first
failing phase, `auth-home`, asserted that a pending resumed read keeps Home
showing, the behaviour the owner rejected in [[BPR-0795]]. `2fbcc8cb`
corrects that assertion: Overview's loading state shows and Home stays one
enabled gesture away. `auth-home` and `auth-stale` then pass, and
the course reaches phases that fail for a real reason. A diagnostic driver ran
every phase and continued past failures
(`build/rfc0025-closeout-restoration-phases.log`, driver
`build/rfc0025-closeout-restoration-phases.py`; the same phases at `e2f6eb1d`
differ only in the two corrected ones,
`build/rfc0025-closeout-restoration-phases-baseline-e2f6eb1d.log`):

- `auth-reading-gesture`, `auth-reading-unchanged`, `auth-reading-home`:
  "gesture precedes deferred reading restoration completion".
- `unavailable-semantic`: "unavailable reading reports explicit failure"
  (destination and sibling reading kept; the explicit report is missing).
- `auth-restore`: "restart preserves exact anchors, commit OIDs, file and
  complete semantic identity". A field-level diagnostic shows the comparison
  (both anchors and OIDs) and the file restore exactly and **only the semantic
  selection differs**. `auth-close` and `auth-verify` depend on it and are not
  reached.

These are RFC-BPR-0024 reading-restoration behaviours (BPR-0709/0710, Done),
not RFC-BPR-0025 slice work. They leave AC-05's surviving reading-position
restoration and AC-06's explicit unavailable-reading report without passing
direct evidence, so this closeout does not claim them and RFC-BPR-0025 is not
closed. Resolving it needs either a repair card for the reading-restoration
regression, allocated by the main integrator and added to this card's
dependencies, or an explicit owner amendment. Neither is supplied.

**Gates at the closeout head.** `verify_documentation.py`, `git diff
--check` and `issue_board validate --rfc RFC-BPR-0025` are recorded under
Current checkpoint.

## Current checkpoint

- Candidate: `2fbcc8cb` plus this card's bookkeeping commit; branch
  `codex/rfc-0018-0026-closeout`, not pushed, not merged.
- Changed by this milestone: `src/adapters/qt` (resident filter, repositories
  drawer, files tree, workbench window), `tests/adapters_qt` (four courses and
  the new selectors), `tests/application/workbench_application_host_tests.cpp`,
  `tests/end_to_end/profile_window_review_restoration_public_path_tests.cpp`,
  `docs/ux/workbench-specification.md`, `docs/ux/appearance-and-accessibility.md`,
  the BPR-0981 and BPR-0597 cards, and the RFC-BPR-0025 status note and index row.
- Evidence files (build tree, not committed): `build/rfc0025-integration-full*.log`,
  `build/rfc0025-bpr0981-*`, `build/rfc0025-closeout-*`.
- Gates: `verify_documentation.py --root .` is `DOCUMENTATION_OK` (run with
  `PYTHONPATH` set to the installed implement-rfc scripts, since this worktree
  has no `.agents` link), `git diff --check` is clean and `issue_board validate
  --rfc RFC-BPR-0025` is valid over 973 cards, at `1275188f` and again after
  this card moved to Blocked.
- Pending obligations: AC-05 reading-position restoration and AC-06 explicit
  unavailable-reading report (blocker above). AC-04 stays superseded and
  unscheduled: not built, not passed. No attended verdict is required by the
  surviving criteria and none was prepared.
- Next action: the coordinator obtains a repair card for the reading-restoration
  regression or an owner amendment. With a repair, rerun
  `application_profile_window_review_restoration_public_path` (all phases) and
  the AC-06 inspection at the new head, reuse the unchanged rows above, then
  close RFC-BPR-0025 and the index. RFC-BPR-0026 stays unstarted until then.

## Merged into main, 2026-09-22

The branch merged into main at `6a897af8` (parents `5b14d7e8`, `317f0023`);
the main integrator's follow-ups are `1eab67a7` (allocation), `d99aeda7`
(cards), `827ca5f6` and `6548bca7` ([[BPR-0984]]). Supersedes the checkpoint's
"not merged" line and its next action.

- Rows that re-pass at `6a897af8` (`build/merge-0018-0026-integration.log`):
  AC-02/AC-09 `qt_files_tree`, `qt_repositories_drawer`,
  `qt_composed_resident_drawer_filters`, `qt_workbench_focus_order_scale_100/125/200`;
  AC-07 `application_workbench_host` and the two-account and multi-window
  courses; AC-08 `application_review_ledger_composition_contract` 38.12 s and
  theme customization 26.26 s; restoration core/host/storage tests.
- Resolved at the merge: the `application_review_round_trip_composition_contract`
  pin drift listed above (resealed in `6a897af8`; review of `6f8e44e4` on
  [[BPR-0978]]). `application_workbench_composition_contract` was a real
  regression from `9210330a`, not an environment failure; [[BPR-0984]] fixed
  its probe at `827ca5f6`. In the main checkout at `6548bca7` the application
  public-path, device-authorization and anonymous-preview rows,
  `application_render` and `application_appearance_store_fallback_contract`
  pass (`build/merge-0018-0026-public-family.log`), so their failures in the
  closeout worktree came from that environment. `application_uiforetw_selected_diff_contract`
  (`UIFORETW_CAPTURE_LAYOUT_FAILED:985:835`) and `qt_anonymous_public_preview_scale_*`
  fail identically on main before the merge and on the branch tip: pre-existing
  and outside this closeout.
- Still red, unchanged: `application_profile_window_review_restoration_public_path`
  fails at `auth-reading-gesture` on the merge and the branch tip, and at
  `auth-home` on main before the merge. [[BPR-0985]] is the repair card and is
  now a dependency, so this card moved from Blocked to Backlog.
- Next action: finish BPR-0985, then rerun the restoration course (all phases)
  and the AC-06 inspection at the new head. Reuse the unchanged rows, then
  close RFC-BPR-0025 and its index row. BPR-0985 is not yet in plan
  `ux-redesign-pack-0028`'s `close-rfc-0025` milestone; the next owner amends
  the plan when it binds that milestone.
