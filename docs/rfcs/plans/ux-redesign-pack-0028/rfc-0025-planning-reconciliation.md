# RFC-0025 planning reconciliation — 2026-09-22

Run `rfc-0018-0026-closeout-20260920`, segment `rfc-0025-claude`, milestone
`plan-rfc-0025`; sole non-delegating `rfc-implementer` engineering and
bookkeeping owner in `/Users/jaeger/.codex/worktrees/0152/BetterPullReview`,
branch `codex/rfc-0018-0026-closeout`. The resolved coordinator-implementer
strategy and immutable RFC base `e851eb8a851ab605cf5a85260a1317ecdf66773b`
are unchanged. This continues the ordered pack after RFC-0024's owner
amendment r3; it does not reopen RFC-0018 or RFC-0024.

The user mandate, verbatim: "take over the work in
codex/rfc-0018-0026-closeout, implement RFC-BPR-0024 → 0025 → 0026, land in
main after closeout. Planning stage can revise/update the rfc if it's gone
stale". The coordinator bound that revision authority to the RFC-0024
precedent: record the reconciliation here, allocate needed IDs in the main
checkout and commit only the reservation, and do not remove, weaken or accept
any criterion by planning alone. The owner's run-level disposition stands:
native Windows observation is deferred to standalone Blocked cards, never
blocked on; a Mac attended verdict is prepared and handed off, never awaited
or self-certified.

## What went stale

RFC-0025 was accepted on 2026-09-04. On 2026-09-06 the owner locked
[RFC-BPR-0028](../../RFC-BPR-0028-review-workbench-direction.md), which
"supersedes the layout and chrome sections of RFC-BPR-0025 … where they
conflict" and names browser-style file tabs a non-goal that "stay registered
but unscheduled". That direction shipped: repositories, files, comparison and
comments drawers; the strip with Home, drawer toggles and the four
modalities; the epoch line; the overview. The adopted pack plan (`253a4c87`)
already bounded this milestone to "resident filtering and selection
restoration; browser-style tabs stay unscheduled", and its `close-rfc-0025`
exit accepts "direct evidence or its superseding decision" per criterion.

Nothing here decides that a superseded criterion passed. Each criterion below
is either allocated to a slice, mapped to existing direct evidence that the
closeout must re-run or cite by exact producing commit, or recorded as
superseded or unscheduled under the owner's RFC-0028 lock; the closeout
records that disposition on the row, and the RFC keeps its original wording.

## Inventory at planning head `9097723a`

- `files_tree_model::rebuild` filters by literal case-insensitive path
  substring; `files_tree_view` already restores its selected
  `workbench_file_id` across apply, order and filter without dispatching.
- `repositories_tree_model` has no text filter; `repositories_tree_view::apply`
  resets the model without restoring the current pull request row, and
  `currentChanged` dispatches the open intent, so restoration must be guarded.
- `workbench_window::focus_order()` declares each drawer's tree stop but not
  `filesFilter` or `filesOrderSelector`; the composed focus-order course skips
  undeclared widgets.
- No test types into `filesFilter`; `test_orders_filter_and_the_overview_join`
  covers the model-level substring filter only.
- `core::refresh_epochs_intent` is dispatched only from
  `epoch_comparison_control` (explicit Refresh); no application host polls
  for new uploads, so a named comparison re-resolves only on that gesture,
  and `core_epoch_policy` retains exact explicit pairs through catalog rebase,
  pruning and unavailability without inventing an anchor.
- Per-window pull request, comparison, file and reading-position restoration
  and close-to-Home are RFC-0024's Done BPR-0709/0710 with actual-process
  courses; private ledger retention across close is BPR-0703/0704.
- Drawer independence, rails, per-window modality persistence and the
  Overview drawer refusal are RFC-0028's Done BPR-0646/0651/0662/0721/0724/0727
  with owner accepts recorded on that RFC.

## Criterion map

| Criterion | Disposition |
|---|---|
| AC-01 drawers, compact centre, focus mode | Superseded by RFC-0028 where it conflicts: the Reviews drawer is the repositories drawer, the compact centre is the strip, overview and epoch line, and reversible focus mode is the per-window modality set. The surviving obligation (independently collapsible drawers with window-specific restoration) is verified at closeout by re-running `qt_workbench_window_scale_100` and `application_profile_window_layout_public_path`. No slice. |
| AC-02 fuzzy filters | Allocated whole to [[BPR-0981]]. |
| AC-03 file click and next/previous reuse the view and keep the comparison; Cmd/Ctrl-click and Open in New Tab | Tab gestures superseded/unscheduled. The single-view part is verified at closeout through `core_workbench_interaction` and a composed assertion that the epoch selection is unchanged across file navigation. |
| AC-04 Duplicate Tab | Wholly tab-specific; superseded/unscheduled, not implemented, not passed. |
| AC-05 per-PR tab order and active view across switching and restart; close behaviour without deleting private work | Tab order superseded. Per-window active view restoration, close-to-Home and private-record retention cite BPR-0709/0710/0703/0704 and re-run the window-review restoration selector at closeout. |
| AC-06 no silent retarget; explicit acceptance of named-comparison updates | Tab-scoped update indication superseded. Exact-pair retention and explicit-Refresh-only re-resolution cite `core_epoch_policy` and source inspection, re-run at closeout; BPR-0710 covers unavailable comparisons without substitution. |
| AC-07 late work cannot cross profile/window/PR/tab/comparison; bounded inactive tabs | Tab identity superseded. Profile/window/PR/comparison exactness cites RFC-0024 BPR-0976/0977/0978 and re-runs `application_workbench_host` and `application_workbench_snapshot_observation` at closeout. |
| AC-08 existing diff/appearance/draft/focused-space behaviour intact | "Tabbed workbench" reads as the shipped RFC-0028 composition; re-run `qt_composed_github_scale_100`, `qt_diff_workbench_scale_100` and `application_diff_workbench_public_path` at closeout. |
| AC-09 keyboard, accessibility, long labels, narrow layouts, 100–200% | Tab strip superseded. [[BPR-0981]] declares both filters and the order selector as focus stops; `qt_workbench_window_scale_100/125/200` and `qt_composed_github_scale_100/125/200` are re-run at closeout. |

## Allocation and plan amendment

The configured main integrator allocated exactly BPR-0981 with
`python3 scripts/allocate_issue_id.py --count 1 --reserve-for
rfc0025-planning-reconciliation` in the registered project root; reservation
commit `942fbf66` is incorporated on this branch as `44a733b3`. BPR-0981 is
the only new required dependency of [[BPR-0597]]; [[BPR-0596]] remains the
entry card and closes with this reconciliation.

As in the RFC-0024 precedent, the first mechanical `check-amendment` refused
the new work ID (`amendment must preserve every obligation; reconcile changed
requirements separately`). Under this authorized reconciliation BPR-0981 was
registered as unplanned work and the regrouping into `close-rfc-0025` was
then checked: partial-plan fingerprint
`sha256:b8a5a46f0427c8722956929d4e9d1b2984bf7703aca4f4aa2f41c8345dac4bb3`
to complete-plan fingerprint
`sha256:3d62929096ea6307f475d324b0affd243948e38d6b16b9bbde6c5d47fd77dd1b`,
affected milestone `close-rfc-0025` only (`build/rfc0025-plan-amendment.json`).
The bound `plan-rfc-0025` definition fingerprint
`sha256:6393a232d1d0f600300780d6faf1ac8d0f383bf6bbaf1f524233b151f308f10d` is
unchanged. Semantic acceptance remains an engineering and closeout
obligation, not a helper verdict.

No live GitHub/GHES mutation, credential, push, merge to `main`, or change to
a protected station checkout is authorized. No product code changed in this
milestone.
