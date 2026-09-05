---
schema: "implement-rfc.card.v1"
id: "BPR-0043"
title: "Implement app-owned bare caches and retained private refs"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0042"]
---

# Implement app-owned bare caches and retained private refs

Create and reuse versioned app-data bare repositories under exact validated
profile-revision/host/account/repository/object-format authority. Do not share
a physical object database across accounts or profile revisions. Reject
symlink, junction, ownership, manifest, configuration, object-format,
alternates, shallow/graft, worktree, or non-bare drift before use.

Request only the exact accepted base and pull-request-head advertisements
through the app-owned protocol-v2/native-HTTPS path, import only a bounded
pack through the credential-free Git process boundary, verify both expected
OIDs and complete commit connectivity, and atomically retain paired base/head
private refs. The catalog allocates one opaque repository-cache scope for the
complete physical partition and a separate opaque PR-authority scope for
every retained and staging ref, so PR numbers or ordinals cannot collide.
Never add a remote, fetch wildcard refs/tags, use provider URLs as authority,
or use remote `HeadRef` text in an argument or refspec.

This slice owns the app-owned protocol-v2 client, native-HTTPS smart
transport, request-owned spool creation and completed-spool validation, exact
pre-import pack inventory and preexisting-pack observations, cache locking,
cache reuse, local import handoff, connectivity checks, and retained private
refs. It must expose the exact validated spool/pack/inventory facts needed by
the durable BPR-0044 operation record. BPR-0044 owns persistence of those
facts; BPR-0045 owns the required journal-before-`index-pack` sequencing.

Prove cache reuse and force-pushed old-head residency after the fixture remote
removes and prunes the old object. Hard quota exhaustion must fail without
silently deleting retained history. Repository deletion and ref pruning remain
explicit, exact-target operations with documented consequences.

## Committed component evidence

- Commit `d903290` implements the bounded app-owned protocol-v2 acquisition.
  Discovery and `ls-refs` are whole control replies bounded to 64 KiB; fetch
  is incrementally consumed rather than retained as one response.
- Commit `edd762e` implements the native smart-HTTPS wire and lease boundary.
  Native fetch reads are at most 16 KiB. GitHub.com smart Git uses
  `github.com`, not its `api.github.com` REST/GraphQL origin; GHES uses its
  exact validated enterprise hostname.
- Commit `b65b0a9` hardens the credential-free installed-Git process boundary
  with exact cleanup identities and cross-instance mutation serialization.
- Commit `2801241` implements the authority-partitioned bare cache, private
  request-owned pack spools, bounded inventory/import handoff, paired staging
  and retained refs, exact prune/delete operations, and stop-aware cache
  lifecycle. Repository, lock, spool, installed-pack/index, and deletion
  decisions are bound to exact filesystem identities and preserve substituted
  or otherwise unprovable state rather than deleting it.
- The application port exposes versioned, scheme-tagged fixed-size no-path
  identities, roles, and sizes for the completed spool and installed
  pack/index, plus canonical bounded pre-import inventory facts for BPR-0044.
  Its opaque authorization tokens remain in-memory state.
- The focused replay passed 5/5:
  `git_protocol_boundary`, `application_git_protocol_v2`,
  `native_git_smart_transport_contract`,
  `native_git_smart_transport_fixture`, and `git_process_contract`.
- The native loopback verifier executes 19 GHES scenarios plus the GitHub.com
  discovery, `ls-refs`, and fetch success scenarios, for 22 isolated child
  runs without contacting GitHub.com or a GHES service.

Production source policy is a 15-minute total smart-operation deadline and a
30-second no-progress idle deadline. The compiled contract asserts the total
policy, while the timeout fixture uses a reduced test-only absolute deadline.
It does not directly establish 30 seconds of elapsed idle-time behavior.

## Completion evidence

- `git_cache_contract` passed 1/1 in 162.58 seconds against commit `2801241`.
  It covers SHA-1 and SHA-256 caches, exact authority isolation and reuse,
  manifest/topology/configuration validation, true cross-process writer
  exclusion, lock and repository substitution, spool lifecycle races,
  pre-import inventory, quota refusal, cancellation, import reconciliation,
  paired ref promotion/prune, identity-bound tombstoned deletion, and
  preservation of uncertain or substituted artifacts.
- The fixture retained the old head before force-pushing and pruning the
  fixture remote, directly proved the old remote object absent, imported the
  replacement, and then proved the original commit and its complete object
  connectivity remained available from the local cache.
- The adjacent protocol/native/process/architecture CTest replay passed 7/7
  in 92.95 seconds. The standalone protocol boundary suite passed 3/3, and the
  architecture mutation suite passed 104/104.
- A tests-disabled Release build contained zero cache fault-injection markers.
  The documentation validator passed, tracked and on-disk GitHub Actions
  workflow counts were 0/0, and `git diff --check` passed.
- A corrected-snapshot independent review completed cleanly. The conservative
  simplification pass made no code change because the explicit validation,
  lifecycle, and cleanup guards encode the cache's fail-closed invariants.

This is direct cache/ref component evidence. It does not prove that the public
BetterPullReview composition selects the path, that the authorization-shaped
values are durable, or that interrupted work recovers across restart.

BPR-0044 owns durable operation facts. BPR-0045 owns
journal-before-`index-pack`, ref/catalog orchestration, stale-result
publication rejection, and recovery sequencing. BPR-0047 owns public
composition, and BPR-0053 owns target-native Windows execution. The exported
artifact and inventory facts are serializable inputs, but their presence is
not evidence that BPR-0044 has durably recorded them or that BPR-0045 crash
recovery is complete.
