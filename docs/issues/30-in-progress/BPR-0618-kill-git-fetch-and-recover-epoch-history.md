---
id: BPR-0618
title: Kill Git fetch and recover durable epoch history
status: In Progress
rfc: RFC-BPR-0013
depends_on: [BPR-0616]
---

# Kill Git fetch and recover durable epoch history

RFC-0013 requires process-kill recovery of Git cache and SQLite together.
Extend the existing real epoch-maintenance fixture (production application
host, installed Git cache, SQLite catalog, fake smart-HTTP bytes) to retain its
private roots across abrupt death and restart through normal host acceptance.

## Acceptance and ownership

- Kill a live child during a partial pack fetch, after complete spool
  acquisition, after installed-Git revision preparation, and after the first
  visible epoch. Use existing transport, diagnostics and projection boundaries;
  no production-only test switch or parallel recovery implementation.
- Reopen through a fresh production host. Require exact commit comparison,
  one retained visible epoch with a matching Git ref pair, no duplicated
  history on another restart, and completed/aborted recovery operations.
- Independently inspect Git object/ref integrity, SQLite integrity, and owned
  temporary spool cleanup. Any recovery defect found is part of this slice.
- Bound checkpoints, forced kill and child cleanup. No network, credentials,
  sign-in, GitHub mutation, Actions, push or paid CI.

Owned paths: `tests/git_contract/epoch_maintenance_integration_tests.cpp`,
its CMake registration, a focused `tests/scripts/epoch_kill_recovery_tests.py`,
shared process-kill support extracted from the BPR-0616 harness if useful,
and the isolation verifier's matching selector entry. Production fixes, if
needed, stay in the actual epoch/cache recovery owner and are recorded here
before expanding verification.

## Verification

Build the existing epoch-maintenance test target. Run its retained prune/delete
selector and new `epoch_kill_recovery`, plus the BPR-0616 restart selector if
shared harness code changes. Run the isolation manifest check for the new
selector, scoped formatting, documentation gates and diff review. These are
persisted-state public paths; no GUI change or appearance approval is implied.

This covers host-operation boundaries and partial fetch, not power loss or
interruption inside SQLite's transaction engine. AI-session kill and remaining
RFC-0013 resource/performance/fuzz criteria remain open in BPR-0617.

## Reproduced recovery gap — 2026-09-05

The new `epoch_kill_recovery` lane uses the real host and concrete cache/catalog
adapters. It confirms a 115-byte receiving file at the partial-fetch kill;
the full pack fixture is 230 bytes. Other checkpoints stop after protocol
acquisition returns a completed spool, after real `prepare_revision` returns,
and on the first visible-epoch projection. The parent verifies SIGKILL status;
production storage opens the killed state before independent SQLite reads.

The initial four-selector run passed the network-isolation manifest (3.45 s),
BPR-0616 review restart (0.42 s), and real prune/delete (21.40 s), then exposed
the first new recovery failure. The expanded harness runs all four independent
courses: **visible passes; fetch, spool and prepared fail**, each with
`EPOCH_CACHE_FAILURE:13` (`git_cache_failure::repair_required`). That full
four-course run took 46.95 s. This is an open regression, not a passing gate.

Source-backed cause:

- `installed_git_cache.cpp` registers the receiving spool's exact file identity
  only in the process-local `Spools` vector. The host calls `record_spool` only
  after full protocol acquisition and cache inventory observation.
- `inspect_operation_recovery_locked` classifies any receiving file, or a
  completed file without a durable spool expectation, as ambiguous.
- `prepare_revision` imports objects and staging refs before its exact install
  facts reach `record_install`. A crash at that boundary leaves an installed
  pack without durable file-identity evidence; it is also ambiguous.
- `abort_recovered_operation` refuses ambiguous ownership. `recover_append`
  records repair-required state, preventing normal startup for that scope.

The refusal protects unrelated files; it must not be converted to a broad
filename-based delete. The next implementation must retain exact ownership
facts across these effect/journal gaps, following the existing typed recovery
protocol. Assess the receiving-sink and install-publication boundaries before
choosing the minimal durable representation. Any extension belongs in the
existing application contracts/epoch host and Git/SQLite adapters, with direct
wrong-identity, substituted-file, restart and cleanup checks. Keep unrecorded
or preexisting artifacts protected as required by `git-and-epoch-model.md`.

The candidate also records the full visible epoch identity before death so the
passing post-visibility course must preserve it, not reconstruct a replacement.
A second restart must retain the same Git refs and one catalog epoch. This
strengthening is included in the final candidate replay recorded below.

Shared `process_kill_support.py` contains the unchanged bounded child runner
and kill helper extracted from BPR-0616. No production source changed yet.
All fixture roots are contained temporary directories and all children are
reaped. No separate build root, live service, credential operation, Actions,
push or paid CI was used. Scoped formatting, simplifier inspection,
documentation validation and all 62 documentation tests pass. Existing
unchanged product gates are retained; this card remains In Progress until the
new direct regression passes for all named boundaries.

Final strengthened candidate replay: `ctest --test-dir build/dev
--output-on-failure -R '^epoch_kill_recovery$'` reproduced the same three
failures in **47.58 seconds**; the visible course passed its full epoch-identity,
second-restart, Git fsck/ref, SQLite integrity and spool-cleanup assertions.
The fixture target rebuilt successfully after adding that identity assertion.
No negative test result has been relabeled or disabled. Resume BPR-0618 at the
exact-ownership repair described above; the next ID is BPR-0619 if a separate
bounded dependency is needed.
