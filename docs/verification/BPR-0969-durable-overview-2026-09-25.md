# BPR-0969 durable overview slice — 2026-09-25

## Assignment and scope

Assignment `/root/bpr0969_durable_snapshot`, mandate
`r4-standalone-durable-snapshot-20260925`, baseline
`0810d10e53510baf011deced3330aebfda2d9b12`, exclusive writer on
`/Users/jaeger/Documents/GitHub/BetterPullReview` main. This native Codex
`rfc-implementer` used implement-rfc coordinator-implementer revision 2 and
adopted the `durable-cached-overview` milestone in the existing plan. Coordinator
approved the bounded overview-only slice. Shared preflight remains
`incomplete / state.drift / host_load=not_verified`; no tool-state repair,
MASK readiness or observed serving claim is made.

This completes the four bounded exits, with partial evidence and repair for
canonical AC2/AC3. It does not accept those whole criteria. The
[card](../issues/30-in-progress/BPR-0969-verify-cached-review-offline-restart-freshness.md)
returns to Backlog; the
[handoff](../rfcs/plans/bpr0969-offline-inventory/durable-snapshot-handoff.json)
records exact source, commands, ownership and continuation.

## Implemented contract

- A validated provider snapshot accepted by the core schedules a worker-only
  save of title, body, author, refs, counts, state and exact base/head commit IDs.
  Cache content never enters the accepted live-snapshot slot.
- The profile-private `state/v1/review-overviews.sqlite3` key binds complete
  host-profile authority (including revision), profile/host, numeric accepted
  account, repository locator and PR number. Numeric repository/PR IDs are
  retained and checked when the request supplies expected IDs.
- Admission requires current-process authenticated identity, the exact current
  request/session/generation, exhausted transport retries, HTTP status zero,
  no live accepted snapshot and a matching valid record. Permission errors,
  anonymous access, cancelled work, retired identity and foreign/corrupt records
  expose no cached metadata. SQLite load/save stays on the shared serial effect
  lane; the decision lane rechecks admission and drains outstanding completions.
- The visible Overview says “Cached overview · freshness unknown,” exposes
  exact stored base/head IDs in tooltip/accessibility description, and names
  source, file catalog, conversations and review state as unavailable. It grants
  no epoch, source, mutation or markdown-link authority. Reopen online obtains
  server truth and clears the cached marker. Home can return to the cached view;
  another read clears the prior destination's cache marker.
- Versioned strict schema, bounded decoding and SHA-256 integrity refuse damaged
  records. Ten entries share a profile; payload ≤272 KiB, body ≤256 KiB,
  database ≤2048 pages. Writes replace atomically and evict oldest saved entries.
  Cancellation after successful COMMIT reports durable success. The profile purge
  allowlist includes this database and its sidecars.

The cache is optional: a window permits one outstanding save and can skip a
later accepted observation during it. It therefore promises a previously
persisted observation, never the newest server state. SHA-256 detects record
corruption; the owner-private store remains the local trust boundary.
A disconnected locator lookup cannot prove current ownership after repository
rename/reuse or the current server head. The view makes only a historical claim;
it does not grant current numeric repository or revision authority.

## Verification and controls

All public courses use synthetic account/PR data, a synthetic vault passed only
through bounded pipes, owner-private temporary roots, macOS network denial,
real application composition, production SQLite and installed Git. Each phase
is a separate process. Public Quit must drain services before exit. Retained
Git pack bytes and catalog authority, exact revision and endpoint anchors remain
unchanged across restart phases. No live account or private repository is used.

1. Original baseline inventory passed (24.06 s). With the new cached-overview
   public assertion but baseline product sources/CMake restored from `0810d10`,
   the course failed in 19.42 s at “durable cached overview is actually visible
   after offline reopen.” Candidate product bytes were restored after exit 8.
2. The first candidate public course caught a real missing Overview visibility
   condition; it was corrected. Focused core/host/store tests passed, then both
   composed inventory/account courses passed (86.20 s). This is diagnostic
   history, not the final candidate claim.
3. Owning-layer controls cover exact account/authority/destination/PR and numeric
   identity mismatch, invalid OIDs, oversize body, eviction, corrupt digest,
   unknown schema, cancellation including late COMMIT, stale generation,
   permission failure, dishonest cache-port output, shutdown and reconnection.
   Purge verifies the overview database and WAL are removed.
4. Final integration includes actual restart into an offline cached view,
   both account directions, delayed old-account completion, exact revision
   labeling, a separately corrupted-cache restart, refused foreign destination,
   fully disconnected startup refusal, and recovery to accepted server data.
   The existing network-loss course also verifies changed server conversation
   truth and unchanged exact retained source identity on reconnection.

Source candidate: `7da4f8f3b316668180bdd122b5fafbebf74c7e67` (implementation
`220e2b1962321e381ef9a51febccf520dca5e221` plus an explicitly unsigned decode
operation found by scoped clang-tidy). Intentional plan/claim commits:
`c038cf35e5b03cbbf60565ca8f25251573d95742`,
`0cf4dbffedf2cf351c04dcf4d46c0efb9ed8d413`.

| Verification | Result and exact scope |
|---|---|
| Final rebuilt candidate, seven focused/public checks | 7/7 pass, 95.08 s; inventory 30.35 s, account isolation 60.98 s, core .05 s, host .06 s, purge .38 s, SQLite overview .27 s, Qt 2.98 s |
| Integration before cast-only tidy correction | 9/9 pass, 222.60 s; includes architecture boundaries 24.64 s and boundary contract 101.82 s; those unchanged boundaries are reused |
| Core-only configure/build and checks | 3/3 pass, 126.80 s; core read policy and both architecture checks; core bytes unchanged by the SQLite cast correction |
| Network-isolation manifest | 1/1 pass, 6.68 s |
| Formatting/static checks | 19 touched C++ files pass clang-format 23.1.1; new core policy/SQLite serializer pass clang-tidy with warnings as errors; `git diff --check` passes |
| Documentation | `DOCUMENTATION_OK`; 64/64 regression tests pass in 1.834 s; scoped board valid (1,186 cards) |
| Shared milestone consistency | `action=stop`, `engineering_acceptance=not-evaluated`; four bounded exits pass, canonical AC3 pending |
| Rendered fixture | Final cached account-910001 Overview inspected, exact revision tooltip asserted; corrupt-cache Home inspected. Qt offscreen/macOS arm64; no human/native verdict |

Commands ran from the source root, without output pipelines:

```sh
cmake --build --preset dev --target test_offline_inventory_public_path test_better_pull_review_application_github_window_read_host_tests test_better_pull_review_github_window_read_tests test_cached_review_overview_storage test_release_maintenance_files test_better_pull_review_qt_github_tests
BPR_OFFLINE_RENDER_DIR=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0969-durable-snapshot ctest --preset dev -R '^(application_offline_inventory|application_recent_account_isolation|sqlite_cached_review_overview|sqlite_release_maintenance_files|core_github_window_read|application_github_window_read_host|qt_composed_github_scale_100)$' --output-on-failure
ctest --preset dev -R '^test_network_isolation_manifest$' --output-on-failure
cmake --preset core-only
cmake --build --preset core-only --target test_better_pull_review_github_window_read_tests
ctest --preset core-only -R '^(core_github_window_read|architecture_boundaries|architecture_boundary_contract)$' --output-on-failure
python3 _coding-standard/scripts/tidy.py --root . --build-dir build/dev --warnings-as-errors --paths src/core/src/cached_review_overview.cpp src/adapters/sqlite/src/cached_review_overview_store.cpp
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
```

The larger nine-test integration used the same final-course selector plus
`architecture_boundaries|architecture_boundary_contract` before the SQLite
cast correction. The handoff hashes all changed source paths, the rebuilt public
executable, and retained runtime logs/rasters. It records the shared milestone
consistency result separately from engineering acceptance. Task-owned temporary
product backups and the adopted proposal were removed; safe evidence remains
ignored under `build/bpr0969-durable-snapshot/`.


## Remaining obligations

- Startup while `/user` is unreachable remains unavailable: a persisted credential,
  Recent row, overview or Git/catalog entry cannot authenticate a current account.
  The next milestone must define the offline account-admission contract first.
- Durable source, file catalogs and conversations are not supplied by this slice;
  unavailable is not an observed empty result. Cached overview review-state counts
  remain unavailable. Full cached-review AC2/AC3 are unfinished.
- Exact changed-head/base recovery after disconnected restart, repository
  rename/reuse/revocation semantics for a full snapshot, and broader schema,
  crash/concurrency/native platform evidence remain outside this bounded course.
  A permission-denied request cannot show a cache, but this slice does not claim
  deletion of all prior historical rows upon permission revocation.
- Qt offscreen rasters are agent-inspected fixture evidence. Human acceptance,
  native compositor/accessibility behavior and Windows/Linux runs are pending.
  No new attended session was authorized or created; protected RFC0052 candidate,
  session, server and waiter remain unchanged. No release qualification, remote
  mutation, push, Claude Code or delegation occurred.
