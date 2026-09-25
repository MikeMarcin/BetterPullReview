---
schema: "implement-rfc.card.v1"
id: "BPR-1131"
title: "Document tabs in the core and the host"
status: "Done"
rfcs: ["RFC-BPR-0052"]
depends_on: []
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner decision on BPR-1106, 2026-09-24","design":"docs/design/BPR-1106-scaffolding-v8.html","criteria":["AC-05","AC-06"],"reservation":"docs/issues/_reservations/BPR-1129--BPR-1135--89a65c75ff1a.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 Core transition tests cover every operation, and host tests cover per-tab completion routing and stale rejection.","outcome":"pass","revision":"e94e67f1","evidence":{"kind":"command","detail":"macOS 26.6.2 arm64, build/dev Debug at e94e67f1: core_document_tabs passes (open in active tab, open in new tab, duplicate, close with the neighbour rule, close others, activate, next/previous with wrap, return to the window's comparison, make the tab's pair the window's, select and set comparisons, the bound, unavailable views, readings, refresh, restoration). application_document_tab_host passes (A/B/A completions out of order, a closed loading tab, duplicates, a refresh, leaving the pull request, shutdown). Control 1 (complete_view accepting any request) fails both suites; control 2 (previous-neighbour rule) fails 'closing the active tab selects its next neighbour'. core-only preset: 40 of 40 rows pass."}},{"criterion":"AC2 Restoration tests keep tab order, the active tab, each tab's comparison and position, and the window's comparison.","outcome":"pass","revision":"e94e67f1","evidence":{"kind":"command","detail":"core_document_tabs test_restoration_keeps_order_active_pairs_positions_and_the_window (persisted/restore round trip with fresh identities, four refusals); core_window_review_restoration test_document_tabs_follow_the_reading; application_window_review_restoration test_document_tabs_persist_and_resume (record and recently viewed entry, Resume after restart); sqlite_window_review_restoration_storage test_tabs_round_trip, test_tabs_bound (largest valid file), test_tabs_refusals (eleven corruptions refused without writing), test_tabs_migration (v3 to v4). All pass at e94e67f1; application_profile_window_review_restoration_public_path passes (585 s at the pre-rebase candidate a0349060)."}},{"criterion":"AC3 Two tabs on one file with different pairs stay distinct and exact through refresh and restart.","outcome":"pass","revision":"e94e67f1","evidence":{"kind":"command","detail":"core_document_tabs test_two_tabs_on_one_file_stay_distinct_and_exact (duplicate, own pair, reload_active keeps both pairs and retires the pre-refresh load, persisted and restored exact); application_document_tab_host test_two_views_of_one_file_through_refresh_and_restart (refresh under a fresh request with the reading kept, the pre-refresh completion refused, a restarted host loads the active tab's own pair and publishes both views distinct, in order); sqlite round trip with two tabs of one file on different pairs. Windows and Linux station runs are owed and recorded on the card with the exact command."}}]
---

# BPR-1131: Document tabs in the core and the host

## Outcome

The core holds, per window and pull request, an ordered tab collection. Each
tab has an identity, a file, a comparison and a reading position. The window
has its own comparison, which new tabs take. RFC-BPR-0025's operations become
typed transitions: open in the active tab, open in a new tab, duplicate,
close with the neighbour rule, activate, return a tab to the window's
comparison, and make a tab's pair the window's. The restoration store keeps
the collection across restart, as it keeps the single view today. The host
routes every completion to the tab that asked for it, so a late completion
cannot populate another tab. A tab whose exact view becomes unavailable says
so rather than being retargeted.

## Seams

- The window state and restoration of RFC-BPR-0024.
- The epoch and comparison projections the Updates line reads.
- The selected-file and reading-position state today's single view uses.

## Acceptance

- Core transition tests cover every operation, and host tests cover per-tab
  completion routing and stale rejection.
- Restoration tests keep tab order, the active tab, each tab's comparison and
  position, and the window's comparison.
- Two tabs on one file with different pairs stay distinct and exact through
  refresh and restart.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-05, AC-06 as drawn in [round v8](../../design/BPR-1106-scaffolding-v8.html). The
core decides, the host performs, Qt presents. It changes no publication,
authority or review semantics and no provider, Git or storage capability. It
starts in canonical order, after RFC-BPR-0028 to RFC-BPR-0038 close. Run the
focused courses named here and the documentation gate, and record exact
commands, results and the producing revision. The owner's attended look at
the built chrome on macOS and Windows is evidence this card cannot supply by
itself.

## Done on macOS, 2026-09-25 (main, `e94e67f1`)

The owner activated RFC-BPR-0052 on 2026-09-25 ahead of canonical order (see
the RFC's status section), so this slice started without waiting for
RFC-BPR-0028 to RFC-BPR-0038.

### What changed

- **Core.** [`document_tabs.hpp`](../../../src/core/include/bpr/core/document_tabs.hpp)
  holds one window's tabs for the pull request it shows:
  `WindowComparison` (the exact pair new tabs take), the ordered tabs (an
  identity and an exact view: pair, file, reading position), the active tab,
  and the one load in force. The bound is sixteen tabs
  (`core::maximum_document_tabs`). RFC-BPR-0025's operations are typed
  transitions: open in the active tab, open in a new background tab (after the
  active one, on the window's comparison, not loaded), duplicate (exact view,
  activated beside its source), close (the next neighbour, or the previous one
  when it was last; the final tab leaves the pull request open with its
  comparison), close others, activate, step next or previous with wrap, return
  a tab to the window's comparison, and make a tab's pair the window's.
  `complete_view`, `observe_reading_position`, `reload_active`, `restore` and
  `persisted` complete the contract.
- **A decision this card had to make.** When the reviewer chooses a comparison
  for the active tab, that tab carries the window's comparison with it only
  while it shares the window's pair; a tab with its own pair changes alone,
  and sibling tabs keep their exact pairs either way. A reviewer with one tab
  therefore never sees it labelled as different from the window, and nothing
  is retargeted. Choosing a comparison with no tab active (the pull request's
  first, or Overview and Discuss) sets the window's alone. BPR-1132 is where
  this becomes visible; the owner's attended look can revise it there.
- **Host.** [`document_tab_host`](../../../src/application/include/bpr/application/document_tab_host.hpp)
  performs the core's decisions: it asks a `document_tab_view_port` to present
  the active tab, publishes the whole tab projection after each change, hands
  the persisted collection to the restoration owner, and posts every
  completion and reading onto the decision lane. The core accepts one only for
  the load in force (constant-time request comparison), so a late answer for
  another tab, a closed tab or an earlier load is refused and counted. A view
  the port cannot present stays on its tab as `unavailable`, exact.
- **Restoration.** `core::window_review_tabs` travels with the window's
  record and its recently viewed entry, as the reading position does, so
  Resume carries the tabs to the window that asks (`accepted_tabs` in the
  core policy and in `window_review_restoration_host`). Tab identities are not
  durable; a restored collection mints fresh ones in order.
- **Store.** The SQLite restoration store's version four adds
  `review_tab_sets` (scope, owner, window comparison, active tab) and
  `review_tabs` (scope, owner, ordinal, exact view in the version-1 position
  format). Version-three files gain both tables empty in the migration's
  transaction. A set with no owner, a gap in a set's ordinals, an active index
  past its tabs, a row of no set, a tab of another pull request and a
  malformed view are all refused as corrupt without writing. The largest valid
  file (32 records and ten entries, sixteen tabs each) reads within the
  existing bounded read: it passes at a 100-step bound and fails at 60, so the
  unchanged 200 has twice the margin.
- **Not in this card.** Nothing is composed into the window yet: the tab strip,
  the view port over the epoch and workbench hosts, and the Updates line
  showing the active tab's pair are [[BPR-1132]].
- **Architecture.** [system-architecture.md](../../architecture/system-architecture.md)
  describes the tabs beside the window restoration they extend.

### Verification (macOS 26.6.2 arm64, Qt 6.11.2, `build/dev` Debug, worktree at `e94e67f1`)

| Check | Result |
| --- | --- |
| `core_document_tabs` (new): every transition, the neighbour rule, the bound, stale completions and readings, unavailable views, refresh, restoration round trip and refusals | Pass |
| `application_document_tab_host` (new): A/B/A completions out of order, a closed loading tab, a refresh, a restart with two views of one file, refusals, leaving the pull request, shutdown | Pass |
| `core_window_review_restoration`: tabs attach to the destination and its list entry, Resume carries them, a new destination drops them | Pass |
| `application_window_review_restoration`: tabs persist on the record and entry, and a restarted window's Resume brings back both views of one file | Pass |
| `sqlite_window_review_restoration_storage`: round trip, the largest file, eleven refusals, v3 to v4 migration; older fixtures updated for the two new tables and a v5 future stamp | Pass |
| `application_profile_window_review_restoration_public_path`, `profile_window_restoration_fixture_contract`, `sqlite_private_store_open`, `application_workbench_composition_contract`, `application_workbench_host`, `core_workbench_interaction` | Pass (the public path 585 s at the pre-rebase candidate; the rest re-run at `e94e67f1`) |
| `architecture_boundaries`, `architecture_boundary_contract`, `core_dependency_boundary`, `test_network_isolation_manifest` | Pass |
| `core-only` preset: configure, build, and 40 rows (`core_*`, the boundaries, the host and store rows) | 40 of 40 pass |
| Control 1, scratch: `complete_view` accepts any request for the pending tab | Fails: `B's late completion is refused and neither tab changes` (host) and `the view shown before the refresh can no longer complete` (core) |
| Control 2, scratch: closing the active tab selects the previous neighbour | Fails: `closing the active tab selects its next neighbour` |
| clang-format 23.1.1 | Clean on the fourteen touched C++ files (their HEAD versions were clean) |
| `_coding-standard/scripts/tidy.py` | `clang-tidy passed` on the seven new or changed translation units |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

Both controls were scratch edits; the candidate was restored and rebuilt.

### Owed elsewhere: the Windows and Linux runs

The code is portable C++ and SQLite with no platform branch, but it has only
run on macOS. Each station runs, at or after `e94e67f1`:

```sh
ctest --test-dir <build> --output-on-failure -R "^(core_document_tabs|application_document_tab_host|core_window_review_restoration|application_window_review_restoration|sqlite_window_review_restoration_storage|sqlite_private_store_open|test_network_isolation_manifest)$"
```

- **Windows (bpr-main-win): owed.** Expect seven passes.
- **Linux (bpr-main-linux): owed.** Expect seven passes.

A failure on either is this card's defect.
