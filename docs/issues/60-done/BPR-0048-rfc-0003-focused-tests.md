---
schema: "implement-rfc.card.v1"
id: "BPR-0048"
title: "Add RFC-BPR-0003 focused tests and Git fixtures"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0041","BPR-0042","BPR-0043","BPR-0044","BPR-0045","BPR-0046","BPR-0047","BPR-0055"]
---

# Add RFC-BPR-0003 focused tests and Git fixtures

Check in secret-free deterministic Git history recipes and expected manifests
for initial observation, linear and multiple uploads, equal material under
distinct OIDs, non-fast-forward force-push, base-only update, base-branch
change, historical/current-viewer review association, never-observed removed
objects, explicit pruning/deletion, corruption, cancellation, and
cross-authority identity.

Add core replay/property tests, host failure/reordering/recovery tests, real
installed-Git and loopback smart-HTTP contracts, hostile configuration/path/
protocol/ref/pack fixtures, Qt model/selector signal tests, public-path
automation, resource bounds, and sanitizer coverage.

Each test must assert exact authority, OIDs, private refs, catalog revision,
request/generation, availability, changed-set work, and no unintended process,
network, credential, hook, filter, worktree, or full-model-reset behavior.

## Implementation audit frontier

Begin from the complete RFC-BPR-0003 claim matrix and the already verified
component/public targets. Inventory only claims that still lack combined,
hostile, reordered, bounded, sanitizer, or clean-matrix evidence. Reuse the
existing core tables, manual executors, installed-Git/cache/catalog fixtures,
public two-process path, and Qt signal harness; do not add a parallel reducer,
host, cache, catalog, fixture-only application, live GitHub target, or hosted
workflow. Record an explicit disposition when component evidence is already
the closest deterministic surface and add a remediation card rather than
silently weakening an unproved claim.

## Outcome

- Added a manifest-bound deterministic SHA-1 graph whose two review heads are
  siblings of one base. The replacement pack is self-contained and omits the
  old head, so the public seed/reuse contract now proves a real force-push,
  retained old-object availability, restart reuse with zero smart calls, and
  the stable rendered `Force-push/rebase` cause for GitHub.com and GHES.
- Added direct application-host evidence that an unrelated pull-request open
  revokes both the exact smart-authority lease and an already issued transport
  before new provider work, even when the epoch observer throws.
- Added `epoch_history_fixture_contract` to verify frozen pack digests/sizes,
  every commit/tree/blob/parent/identity/timestamp/message fact, independent
  self-contained imports, old-head omission, and sibling ancestry.
- Hardened only test timing: target-native proxy discovery/instrumented startup
  now precedes reduced-deadline cancellation assertions, and the complete
  real-Git process/cache CTest registrations admit slower local hosts.
  Production deadlines and all operation/resource bounds are unchanged.

## Scenario disposition

| Required scenario | Closest deterministic evidence |
|---|---|
| Initial, descendant/multiple upload, equal material, base-only, base-branch change, simultaneous change, unknown lineage, and rehydration | Exhaustive `core_epoch_policy` cause/material table, durable `application_epoch_catalog_contract`, and SQLite behavior round trips |
| Non-fast-forward force-push and remotely absent old head | Manifest-bound sibling graph, independent installed-Git imports, `git_cache_contract` remote prune, both public seed/reuse cases, and rendered ordinal-two cause role |
| Historical/current-viewer and association-only review facts | Core association tables, `application_epoch_host` unchanged/association-only skip-Git path, catalog/SQLite ordered association persistence, public `My last reviewed` selection |
| Never-observed, explicit prune, and cache deletion | Catalog/SQLite availability facts, core maintenance authorization, real Git/SQLite maintenance integration, and three-scale Qt explicit unavailable-state coverage |
| Corruption, hostile paths/config/protocol/ref/pack, cancellation, recovery, duplicate/reordered/stale completion | Installed-Git process/cache contracts, native smart fixture, SQLite storage/behavior contracts, manual-executor host matrices, and architecture/protocol mutation scans |
| Cross profile/host/account/repository/PR/generation/catalog authority | Core identity rejection, host source/authority switching, physical cache/catalog isolation, native lease binding, and the new issued-transport revocation assertion |
| Bounds and sanitizers | Protocol/process/cache/catalog/page bounds plus fresh changed-surface AddressSanitizer/UndefinedBehaviorSanitizer replay; only the production 30-second elapsed idle interval remains intentionally unexecuted |

No remediation card was needed: every required semantic/failure scenario has
direct evidence at the owning core, host, real adapter, or public composition
surface. Native presentation remains BPR-0049, security synthesis remains
BPR-0050, and real Windows execution remains BPR-0053.

## Evidence

- `epoch_history_fixture_contract` passed and bound the exact 466-byte initial
  and replacement packs to their SHA-256 manifests.
- Both public rendered paths passed, and the GitHub.com/GHES two-process
  persistence paths passed with real force-push classification and zero smart
  calls on reuse.
- The focused core/host/catalog/SQLite/Qt/maintenance/architecture group passed.
  The first broad replay exposed three harness timing limits; after the
  test-only corrections, native smart transport passed normally and under
  sanitizers, while the unchanged Git process/cache executables passed in
  104.14 and 456.16 seconds. Their registered CTest ceilings are now 180 and
  600 seconds.
- The final consolidated focused matrix passed 25/25 in 1020.68 seconds. Its
  real Git process/cache cases passed through CTest in 99.73/469.79 seconds;
  GitHub.com/GHES seed/reuse persistence passed in 94.94/101.98 seconds.
- Fresh AddressSanitizer/UndefinedBehaviorSanitizer coverage passed core epoch
  policy, GitHub and epoch hosts, all three Qt scales, native smart transport,
  and the two-process public GitHub.com force-push persistence path. Leak
  detection is unsupported by AppleClang's sanitizer runtime and was not
  claimed.
