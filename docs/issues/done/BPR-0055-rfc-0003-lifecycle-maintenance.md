---
id: BPR-0055
title: Implement core-authorized epoch retention maintenance
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0041, BPR-0043, BPR-0044, BPR-0045]
---

# Implement core-authorized epoch retention maintenance

Add explicit deterministic core intents, transitions, request identity, and
terminal policy for user-authorized epoch pruning and whole-repository cache
deletion. Compose those decisions through the epoch application host to the
existing narrow catalog and Git-cache ports; an adapter method alone is not a
product authorization path.

Pruning must seal the exact epoch/ref pair and expected catalog revision before
deleting refs, then atomically publish catalog availability. Cache deletion
must build and seal the complete bounded affected-catalog set before the exact
repository cache is quarantined/tombstoned. Both flows must preserve
authority isolation, serialized completions, cancellation only before their
durable point of no return, restart recovery, repair-required ambiguity, and
incremental presentation. No automatic or quota-triggered deletion is allowed.

Use manual executors and fault-injected fake ports to reorder every phase,
identity switch, cancellation, duplicate, stale completion, restart, and
catalog/ref failure. Add a real-cache/catalog integration fixture proving exact
cleanup without broad filesystem deletion or false catalog visibility.

## Implementation audit frontier

The BPR-0043 Git-cache and BPR-0044 catalog adapters already expose narrow,
typed prune and cache-delete phase operations, but no active semantic intent
can authorize them and the BPR-0045 host intentionally leaves maintenance
recovery uncomposed. This slice must reuse those durable and physical facts;
it must not introduce a competing deletion algorithm or infer authority from
paths, ref prefixes, adapter availability, or quota pressure.

Implementation begins with the deterministic intent/request/terminal and
compact pending-state algebra. Host orchestration follows only after direct
core tests establish exact targets, revision/generation handling,
pre-point-of-no-return cancellation, mandatory post-point-of-no-return
completion, stale rejection, and incremental deltas. The existing real
cache/catalog contracts remain the physical boundary and will be extended
only where a composed integration claim cannot otherwise fail when false.

## Outcome

- Added explicit prune, whole-cache-delete, and cancellation intents plus one
  exact maintenance request/terminal algebra. Compact core state retains the
  target count and a deterministic domain-separated SHA-256 digest rather than
  a heap-backed target batch.
- Core policy requires the active authority, accepted source, exact
  incarnation/revision/cache, a strictly ordered unique prune batch of at most
  100 epochs, one in-flight action, and a terminal whose complete target
  recomputes to the retained digest. Refresh/authority replacement retires the
  old request; duplicate, stale, tampered, and cross-authority work cannot
  publish.
- The host loads exact anchors and seals the complete cache-delete impact set
  before advancing to the durable point of no cancellation. The durable
  operation ID is the stable authorization token for paired-ref pruning and
  exact cache quarantine/deletion. Restart aborts only pre-point intent and
  resumes every later phase; ambiguity is durably marked `repair_required`.
- Prune publishes exact stable epoch IDs whose availability changed. Cache
  deletion publishes one bulk `cache_deleted` delta. Both retire and
  re-resolve the affected semantic comparison without a projection reset.
- Installed-Git paired pruning is idempotent only when both exact refs are
  already absent. A one-sided or mismatched pair still fails closed.
- Added a real composed fixture using the production core/host, installed-Git
  cache, and SQLite catalog. It proves exact prune tombstones/ref absence and
  whole-cache tombstones/repository absence/cache-scope reuse refusal after
  shutdown and reopen.

## Direct verification

- `cmake --build --preset core-only -j 6` passed.
- `ctest --preset core-only --output-on-failure` passed 27/27.
- `cmake --build --preset dev -j 6` passed, including the new composed target.
- `ctest --test-dir build/dev --output-on-failure -R '^(core_epoch_policy|application_epoch_host|git_cache_contract|epoch_maintenance_integration)$'`
  passed 4/4 in 246.63 seconds. The real cache contract took 220.25 seconds;
  the real Git/SQLite maintenance composition took 26.35 seconds.
- The manual-executor host fixture covers every durable maintenance restart
  phase and injected lookup, point-of-no-return, paired-ref, physical-delete,
  catalog-finalization, and terminal-bookkeeping failures. The core table
  covers invalid targets, bounds, duplicates, stale/tampered terminals,
  refresh, and cancellation on both sides of the point of no return.
- `python3 scripts/verify_documentation.py --root .` reported
  `DOCUMENTATION_OK`; the documentation/architecture CTest gate passed 4/4,
  and the direct architecture mutation suite passed 104/104.
- Tracked and on-disk `.github/workflows` counts were both zero, and
  `git diff --check` passed.
- No GitHub endpoint, credential, remote mutation, GitHub Actions workflow, or
  Windows-runtime claim is involved. Target-native Windows evidence remains
  BPR-0053 work for the product owner's real Windows machine.

## Documentation and simplification

Updated the Git/epoch and system architecture, security threat model,
build/test guide, fixture catalog, RFC-BPR-0003 verification ledger, and live
issue board. The conservative code-simplifier review made no production-code
rewrite: the explicit maintenance phase branches and exact postcondition
checks are the fail-closed recovery policy, while collapsing them would hide
phase-specific authorization or error handling. The focused boundary suite was
rerun after that review.
