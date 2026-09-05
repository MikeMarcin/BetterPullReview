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

## Selected first repair

Close the post-preparation journal gap using the existing SQLite install fact,
without changing its schema. Add a synchronous optional installation witness to
`git_cache_session::prepare_revision`; the concrete adapter offers exact pack,
index, tree and lineage facts before staging refs or deleting the spool. The
selected host must persist and validate those facts in its existing
`record_install` transaction before allowing preparation to continue. Refused
or uncertain acknowledgment cannot trigger cleanup of possibly journaled
artifacts. The host also checks the eventual prepared result against its
persisted fact. The default absent witness preserves lower-level adapter
callers; production host admission always supplies and requires it.

Owned repair paths extend to `git_cache_port.hpp`, `epoch_application_host.cpp`,
`installed_git_cache.cpp`, the host/cache direct fixtures, and the owning
`git-and-epoch-model.md` paragraph. Verify actual callback order/refusal plus
host failure/cancellation and the real killed-prepared course. This does not
claim to repair the earlier receiving/completed-spool gaps or interruption
inside `index-pack`; those remain open in this card.


## First repair verification — 2026-09-05

The selected host now records the exact install fact during the adapter's
synchronous witness, before staging refs and completed-spool deletion. The
host rejects an omitted or repeated witness and a returned artifact identity
that differs from the persisted fact. Direct host fixtures also preserve the
typed failure/cancellation terminal when catalog persistence refuses the fact.
The real Git fixture observes no refs and an intact spool inside the witness;
a refused witness retains the installed pack/index and spool for recovery.
Its disposable cache is cleaned through the normal abort/delete ports after
inspection, before later suite cases run.

The initial prepared restart exceeded the harness's inherited 15-second
review-recovery allowance. A separate contained, network-denied diagnosis
completed successfully in 15.67 seconds, including independent Git/SQLite
inspection. Epoch recovery now has a bounded 30-second child allowance;
review-publication recovery keeps its existing 15 seconds. This changes a
fixture deadline, not a product performance budget.

The five-selector replay ran in 136.20 seconds. Host tests passed (0.68 s),
review-publication restart passed (0.53 s), and real prune/delete passed
(22.23 s). The full epoch kill lane ran in 70.02 seconds: **prepared and
visible pass**, including the second restart, exact retained comparison/ref
pair, Git fsck, SQLite integrity and no unfinished spool. **Fetch and spool
still fail** with repair-required startup. The Git-cache suite initially
caught residue from the new refusal fixture; its abort/delete cleanup was
corrected before the focused replay below.

BPR-0618 remains In Progress. Next repair: persist the receiving file's exact
ownership before accepting pack payload, and preserve that identity across
completion until the catalog's completed-spool record is durable. Recovery
must still refuse substituted or unrelated artifacts; a filename is not
ownership. The gap inside `index-pack` remains outside this first repair.
No production app was launched, and remembered sign-in was untouched. All
new diagnostic/crash roots were removed after their contained consumers ended.
No Actions, remote mutation, push or paid CI was used.


Final focused replay after correcting refusal-fixture cleanup:
`ctest --test-dir build/dev --output-on-failure -R
'^(application_epoch_host|git_cache_contract)$'` passed **2/2 in 201.82 s**
(host 0.68 s; Git cache 201.13 s). The three affected C++ fixture targets
built successfully. The unchanged prepared/visible kill result and
prune/delete/review-restart results above remain valid; no redundant crash
replay was needed for the fixture cleanup and comment/formatting edits.
Documentation validation, all 62 documentation tests, scoped clang-format,
the code-simplifier pass and `git diff --check` passed. No full matrix,
release rebuild, tidy run, GUI/credential check or other-platform execution
was claimed or required for this persisted-state slice.
The changed-port architecture checks also passed:
`ctest --test-dir build/dev --output-on-failure -R
'^(architecture_boundaries|architecture_boundary_contract)$'`, **2/2 in
72.74 s**. Next work remains BPR-0618's receiving/completed-spool ownership
repair; BPR-0617 and the persistent RFC goal stay open.


## Selected receiving ownership repair

Persist a typed receiving-spool ownership fact (operation, opaque file identity,
maximum payload bytes) in the existing SQLite operation journal before the
selected host allows payload consumption. The adapter's receiving-to-completed
rename preserves this identity. Recovery can therefore exact-delete either
name when only this fact exists, without inventing a pack identity or importing
an unrecorded completed spool. Existing completed receipts must agree with it.
Missing legacy facts keep their conservative refusal behavior.

Extend the existing catalog with an additive ownership table and a versioned,
transactional migration that preserves catalog/authority/epoch/operation IDs.
Terminal compaction and cache deletion must also remove the ownership row.
Owned paths additionally include `epoch_catalog_port.hpp`, SQLite catalog and
schema implementation, their direct storage/behavior/migration fixtures, and
application contract checks. Direct verification includes migration rollback,
record/reopen/compaction, wrong/substituted ownership, host refusal, and all four
existing real process-kill courses. No remote or credential effects are needed.


## Receiving repair results — 2026-09-05

`epoch_kill_recovery` passed **107.67 s**, exercising all four forced-death
courses and another fresh-process reopen for each. Each retains exactly one
visible epoch, the exact accepted comparison and paired Git refs, intact
SQLite integrity/foreign keys, Git fsck, and no unfinished spool or recovery
operation. The preexisting visible-epoch identity remains unchanged.

`sqlite_epoch_catalog_storage_contract`, `sqlite_epoch_catalog_behavior_contract`
and `sqlite_anonymous_epoch_authority` passed **3/3 in 41.11 s**. Evidence
includes v1/v2 migration, injected rollback before either migration step's
commit, preserved populated v2 snapshot and unfinished operation, receiving
record/replay/reopen, conflicting identity and payload-limit refusal, and
terminal compaction. The initial storage check correctly rejected its stale
18-table/v2 expectation; the explicit v3 table count and future-version
refusal fixture were updated, and the passing result above is the corrected
replay.

`application_epoch_host`, `git_cache_contract` and
`epoch_maintenance_integration` passed **3/3 in 231.26 s** (0.48, 208.85 and
21.93 s). The concrete Git fixture refuses substituted receiving/completed
files after reopening without in-memory spool registrations, preserves both
the unrelated file and displaced original, and exact-cleans only after the
original identity is restored. Refused creation cleans its owned empty file
without deadlocking; the sink now destructs after the operation lock releases.
Host checks prevent payload acquisition on missing, repeated or foreign
witnesses, storage refusal and cancellation. Prune/delete still passes through
the real host, Git and SQLite adapters.

All affected fixture targets built. Scoped formatting, simplifier inspection,
documentation validation, all 62 documentation tests and `git diff --check`
passed. No separate build root or retained diagnostic root was created;
contained temporary fixture roots cleaned themselves and children were reaped.
The ordinary `build/dev` remains. No GitHub Actions, push, paid CI, remote
mutation, GUI launch, credential read or sign-out occurred.

The tested boundary begins after receiving ownership is committed. Death
between empty-file creation and that commit, interruption inside `index-pack`
or a SQLite write transaction, AI-session kill, and actual power loss are not
claimed by these four courses. Legacy spools without ownership stay protected.
Those broader RFC-0013 boundaries remain open in BPR-0617.

The final changed-port/architecture checks passed **3/3 in 69.00 s**:
`application_epoch_catalog_contract`, `architecture_boundaries`, and
`architecture_boundary_contract`. The source diff was reviewed with the
receiving record, migration, callback lifetime, exact cleanup and terminal
compaction paths together. No known defect remains at this card's four named
kill boundaries.
