# BPR-0969 — Mac offline and restart inventory

## Assignment and candidate

This is the bounded AC1 inventory from the RFC0013 r4 standalone follow-up,
under mandate `r4-standalone-offline-inventory-20260925` and assignment
`/root/bpr0969_offline_inventory`. It completes the
[inventory milestone](../rfcs/plans/bpr0969-offline-inventory/milestone-plan.json).
Full [[BPR-0969]] AC2/AC3 remain pending. The RFC0013 amendment and its earlier
acceptance remain unchanged.

- Source/tracker: `/Users/jaeger/Documents/GitHub/BetterPullReview`, `main`.
- Baseline: `552d0e02c65668b0d49f0050bf0c778e077fc2df`.
- Source candidate: `b3197149a74bbb8030a3d4a534e956b11a64ed9f`.
- Runtime: `build/bpr0969-offline-inventory/`; logs and rasters are local,
  ignored artifacts, not checked-in evidence substitutes.
- Executable: `build/dev/tests/end_to_end/offline_inventory_public_path`;
  SHA-256 `6eb8fceb2da785646e6df259d79799ec649ae12e520878997e06c186b59d7ff6`.
- Mac: Apple M4 Pro, 51,539,607,552 bytes RAM, arm64, macOS 26.6.2 / 25G83;
  Debug, Qt 6.11.2, offscreen, 1500 × 950 QWidget capture. This is rendered
  composition evidence, not native compositor or human acceptance.

## What the actual application retains

`application_offline_inventory` starts the real desktop/profile composition in
three separate processes using one owner-private temporary data root. The
ordinary recorded `profile_window_restoration_v1` provider and installed Git
path supply safe generated data. A test-only shared switch refuses both wire
and smart-Git transport; the enclosing macOS sandbox denies all network access.
The synthetic device-vault handoff stays in bounded process pipes. No live
account, live repository, mutation or release is involved.

| Entry point and condition | Metadata and threads | Source and epochs | Observed meaning |
|---|---|---|---|
| Open exact PR online, then Refresh with transport unavailable | The identical accepted in-memory snapshot remains: two files, two threads, same authority and snapshot id | Same selected comparison key and both source documents; ready retained epoch | Previously loaded content survives this failed refresh. The repaired status explicitly says freshness is unknown. |
| Reconnect and Refresh in that process | New accepted snapshot; fixture changes the first thread root to `Edited server root`; exact account/repository/PR unchanged | Same selected source revision and comparison key | New server observation replaces earlier metadata, and the failed-refresh notice disappears. |
| Open another repository while offline | No accepted snapshot; zero displayed thread rows with an explicit unavailable description | No comparison result or available epoch selector | The preceding repository's content is not used for this failed open. |
| Restart, validate account online, then choose Recent → Resume offline | One exact Recent title/destination exists, but no accepted PR snapshot; Resume attempts provider reads and fails | No source or selectable comparison, despite retained objects/catalog on disk | Recent navigation metadata is not a cached review snapshot. |
| Reconnect after that failed Resume | Original review can be loaded again with exact base/head and observed threads | Installed Git source and retained epoch become usable again | The accepted online observation admits the exact content. |
| Restart with transport unavailable from process entry | Account `/user` observation fails; Home contains no authenticated Recent rows; no accepted PR snapshot | No source or selectable epoch | Vault metadata and disk objects do not independently grant offline account authority. |

After each orderly public Quit, the driver reads the closed private catalog
read-only and compares repository authority, PR scope, epoch identity/material
trees and endpoint anchors byte-for-byte. One retained epoch and two anchors
survive all three processes unchanged. One Git pack also survives with the same
path and SHA-256. These are separate observations: an accepted review snapshot
is in memory; Recent navigation/reading state, Git objects and epoch catalog
records are durable. Durable objects alone do not establish an offline reopen
path.

### Unavailable and empty are different observations

The failed open and offline Resume have **no accepted snapshot**. Their zero
conversation rows accompany `Review threads are unavailable until a pull
request is open.` They are not evidence that the server has zero threads.
Conversely, the loaded review's current file/line filter also displays zero
rows, while its accepted snapshot and visible availability label report two
review threads. Row count alone cannot establish emptiness or availability.
This course observes nonempty server data and unavailable data; it does not
add an accepted zero-thread server fixture or claim that branch was exercised.

### Authority and exact revision boundaries

The generated accepted identity is profile `1`, host `1`, account `1001`,
repository `2001`, PR `3001` (number `17`). Base commit is
`a61a212aa0484929bc5f20a26e9a7c1c6541c726`; head commit is
`3b60c8324eee117ce72e6446fe640d85a60b37a2`. The composed assertions compare full
accepted snapshots across failure, full comparison keys and both documents,
and fresh authority/PR values after recovery. Catalog endpoint anchors match
these commit OIDs; material digests identify trees, not commits.

Source inspection establishes the wider key boundaries:

- `github_window_read_host` owns an optional accepted snapshot in memory.
  A new open resets it; a failed current-target refresh retains it. Accepted
  completion and identity invalidation still go through core validation.
- `epoch_catalog::bind_authority_key` binds profile and revision, host and
  authority digest, account, repository, object format and access mode.
  PR/viewer scope, catalog incarnation, epoch ordinal and exact endpoint OIDs
  further qualify retained selection. Current authenticated authority is
  required before the application can use these objects.
- `window_review_restoration_store` persists destinations, title/time,
  positions, exact comparison/anchor identity and tabs. It does not persist
  the accepted PR snapshot's metadata, bodies, thread collection or file list.
  Exact restoration refuses an authority/incarnation/commit mismatch.
- **Recent title/destination records are profile-private, not account keyed.**
  The table has host/mode and locator fields but no account column.
  `github_application::publish_recent` admits authenticated Recent entries
  after an accepted session on the same hostname, and checks host/mode for
  Resume availability. This cannot establish same-profile account-switch
  title isolation. The current course does not exercise that switch.

The separately run two-account course checks distinct profile authorities,
held-completion cancellation and sign-out without reviving the old account.
It does not fill the same-profile Recent limitation. This inventory does not
claim all account/repository/revision permutations or changed-head recovery.

## Demonstrated repair

Before the repair, the rendered loaded review retained its source and showed
ordinary readiness after a failed refresh; the diagnostic was in the hidden
account panel. Adding the visible-state assertion first failed in 17.30 s:
`failed refresh visibly labels retained data and unknown freshness`.

The sole production edit is in `workbench_window::status_line`: when a review
is displayed, has an accepted pull request and its refresh failed, show
`Refresh failed. Showing previously loaded review data; freshness is unknown.`
The existing status projection triggers the update. No snapshot persistence,
offline authority, retry, Git or epoch policy changes.

The final rendered 1500 × 950 review shows the notice beneath both populated
source panes. The online initial/reconnected and unavailable-open controls
require the notice to be absent. Recovery adopts changed server conversation
data with a new snapshot id rather than merely hiding an error. The existing
private-progress and review-history status remains alongside the notice.

## Verification and limits

The exact source was built with the normal warnings/hardening targets. Final
format check passes for all four touched C++ files, and `git diff --check`
passes. Commands run from the source root:

```sh
cmake --build --preset dev --target test_offline_inventory_public_path test_better_pull_review_qt_github_tests test_better_pull_review_application_github_window_read_host_tests test_rate_limit_public_path test_profile_two_account_isolation_public_path
ctest --preset dev -V -R '^(application_offline_inventory|application_github_window_read_host|application_profile_two_account_isolation_public_path|qt_composed_github_scale_100|architecture_boundaries|test_network_isolation_manifest)$'
ctest --preset dev -V -R '^application_offline_inventory$'
python3 _coding-standard/scripts/format_check.py --root . tests/end_to_end/offline_inventory_public_path_tests.cpp src/app/src/github_application_fixture_access.hpp src/app/src/github_public_path_fixture.cpp src/adapters/qt/src/workbench_window.cpp
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
```

- Focused integration: **6/6 pass**, 65.60 s. Includes cancellation/stale
  completion host controls, Qt projection, distinct-profile account isolation,
  architecture boundaries and network-isolation registration.
- Final three-process course, after adding exact durable catalog comparison:
  **pass**, 24.14 s. Wire refusals: seed 6, authenticated offline Resume 3,
  fully offline restart 3. Git refusals: 0; the current failure path stops
  before Git transport, so this does not observe an active Git outage.
- Baseline affected fixture regression: `application_rate_limit_render_recovery`
  passed in 11.98 s before edits; the modified fixture's default has no offline
  switch. Its broader backoff obligations remain on BPR-0968.
- Documentation: `DOCUMENTATION_OK`; all 64 documentation regression tests
  pass in 1.927 s. Their negative fixtures intentionally print `ERROR` lines.
  Scoped board validation passes with 1,186 cards. The shared checkpoint
  returns `action=stop` and `engineering_acceptance=not-evaluated`; it checks
  consistency rather than supplying engineering acceptance.
- Exploratory harness failures are retained in local logs: wrong test name;
  passing the window instead of the owner to modality setup (compile error);
  requesting Read while requiring its collapsed files drawer; and comparing
  epoch material-tree digests to commit OIDs. Corrected to real Review entry
  and explicit endpoint-anchor assertions. These were harness diagnoses.

AC2/AC3 remain outside this milestone: offline snapshot persistence/admission,
same-profile Recent account-switch behavior, broader revision/authority and
network-loss/cancellation combinations, native rendered/human acceptance and
Windows/Linux evidence. No crash restart, changed-head reconnection, public
preview or zero-thread server result is certified here.

## Next bounded milestone

Diagnose the same-profile Recent account-switch path with a safe two-account
fixture and record the exact metadata exposure boundary. Separately design the
missing durable review observation and offline authority/freshness contract
before implementing cached-review restart. Existing retained Git/catalog
records must not be promoted into offline account authority. Keep AC2/AC3
pending until their complete public paths and required native evidence exist.
