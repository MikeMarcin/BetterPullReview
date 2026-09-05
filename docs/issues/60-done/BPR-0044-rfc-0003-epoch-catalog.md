---
schema: "implement-rfc.card.v1"
id: "BPR-0044"
title: "Implement review-epoch catalog persistence and aliases"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0041","BPR-0043"]
completed: "2026-07-31"
---

# Implement review-epoch catalog persistence and aliases

Implement a narrow host-owned epoch catalog with schema/version authority,
transactions, serialized writes, crash recovery, and exact pull-request and
account scope. Bind every catalog partition, ordinal, and retained-ref identity
to the catalog's opaque repository-cache and PR-authority mappings. Give each
catalog lifetime a nonzero random incarnation so recreated ordinals cannot
collide with stale IDs. Persist ordinal chronology, exact base/head and branch
identities, structured lineage/change facts, deterministic cause, observed
time, provider snapshot, material-equivalence identity, review association,
availability, and retained-ref identity without source bodies or credentials.
Keep this dedicated epoch database separate from RFC-BPR-0007's future review
ledger.

Persist a typed request operation record before local import. It includes the
exact completed spool identity and size, expected pack identity, bounded
pre-import pack-directory inventory, and preexisting-pack fact supplied by
BPR-0043. Recovery must use those exact facts and never infer ownership from a
broad object-directory scan.

Coordinate catalog and private-ref commit so a stale, canceled, duplicate, or
failed job cannot publish an epoch. A different observed revision with
equivalent material remains a distinct epoch. Resolve Branch base, Latest,
Previous upload, My last reviewed, and arbitrary retained anchors to exact
OIDs; never silently remap an explicit unavailable or pruned selection.
Persist a changed newest-current-viewer review association even when the
accepted base/head/branch is unchanged and no epoch is created.

Add bounded/paged history observations and explicit prune/delete transactions.
Automatic retention loss is forbidden.

## Result

Implemented the host-owned BPEC v1 catalog as a repository-pinned, statically
linked SQLite 3.53.4 adapter. The fixed
`state/v1/epoch-catalog.sqlite3` database has application ID `BPEC`, schema
version 1, 18 closed `STRICT, WITHOUT ROWID` tables, 16 closed indexes, a
nonzero catalog incarnation, exact authority mappings, checked ordinal and
catalog revisions, epochs, review associations, retained anchors,
availability, and typed append, prune, and cache-delete operation state. The
BPR-0045 host-remediation slice added the canonical durable-append source-
observation index and checked durable installed-head-lineage fact before
RFC-BPR-0003 closeout.
Every open verifies the complete schema, singleton incarnation, integrity,
foreign keys, compile/runtime policy, fixed path, ownership, identity, and
local-filesystem contract.

The application port and catalog persist exact spool, expected-pack,
pre-import inventory, preexisting-pack, installed head-lineage, ref, OID,
review, source-observation, phase, evidence, operation-revision, and repair-
provenance facts. Restart paging and compare-and-swap transitions preserve one
linearized terminal outcome. Nonterminal revisions reserve the last increment
for terminal or repair state, bounded requests fail without mutation, and
destructive facts remain durable tombstones.

A cache-deleted latest epoch can expose one typed retention-rehydration
requirement. A matching higher-ordinal replacement must carry a different
host-issued nonzero 256-bit source-observation identity; finalization clears
the requirement exactly once and preserves the old tombstone. The catalog
persists identities exactly and rejects catalog-incarnation-wide reuse. It
does not generate or prove entropy; BPR-0045 owns operating-system entropy and
collision retry.

Prune facts admit only `available`, `unavailable`, or `corrupt` starting
anchors and persist the exact base/head availability, OIDs, ref identity, and
expected revision. Begin, replay, finalize, finalized replay, and restart
revalidate those facts. Finalization atomically marks both anchors and every
linked non-destructive review state `pruned` and releases the retained-epoch
quota exactly once.

POSIX bootstrap uses private no-follow, identity-checked, synchronized,
no-replace publication on an admitted local filesystem. The guarded Windows
implementation retains the validated ancestor chain, uses a private exact-name
SQLite VFS, enforces `TokenUser` ownership and a protected owner-only DACL, and
publishes through handle-relative `FileRenameInfo` without replacement.
Windows source was compiled and linked but no Windows binary was executed.

## Completion evidence

- A first-configure CMake defect discovered by the clean matrix was corrected
  by moving the Git-cache fault-injection build into its dedicated contract
  target. The production Git adapter is now seam-free in both test-enabled and
  tests-disabled builds, including an in-place tests-on to tests-off
  reconfigure.
- Fresh macOS arm64 Qt-enabled Debug build: 115/115 targets and 58/58 CTest
  cases passed in 352.14 seconds.
- The focused core, application-port, storage, and behavior contracts each
  passed five consecutive executions: 20/20 total in 21.90 seconds.
- Fresh Qt-free Debug build: 47/47 targets and 25/25 CTest cases passed in
  41.31 seconds.
- Fresh ASan/UBSan focused matrix: 4/4 passed in 23.70 seconds with leak
  detection disabled for the macOS run.
- Fresh tests-disabled Release build: 49/49 targets, zero registered tests,
  zero catalog test-seam symbols/strings in the SQLite adapter archive, static
  vendored SQLite selected, no dynamic SQLite dependency, and the binary
  dependency audit passed.
- Fresh MinGW x86-64 cross-build: 47/47 compile/link steps passed under strict
  warnings; CTest inventory was 25, with exactly the two catalog contracts
  labeled `BPR-0053` and `target-native-windows`. Both outputs were PE32+ x64
  executables with statically linked SQLite. No Windows executable ran.
- Core and architecture CMake scans passed; boundary mutation suites passed
  4/4 and 104/104; platform configuration passed 4/4.
- Independent correctness review resolved source-ID restart reuse and
  unavailable/corrupt-prune gaps, then returned clean. The required
  behavior-preserving simplification pass reused the common authority binder
  and removed one unused Windows VFS field; its final review was clean.
- The documentation gate, workflow-absence scan, tracker graph, and
  whitespace checks passed on the closeout tree. Tracked and on-disk GitHub
  Actions workflow counts were 0/0.

## Documentation disposition and deferred boundaries

Updated the Git/epoch architecture, security model, build/test guide, fixture
catalog, dependency baseline, vendored SQLite contract, issue board, and
RFC-BPR-0003 verification ledger. README, AGENTS, product contract, system
architecture, and UX ownership were reviewed with no BPR-0044-specific change:
the catalog is not yet selected by the public application.

BPR-0041 owns deterministic semantic alias and cause policy. BPR-0045 now
supplies OS-entropy source-observation generation, real spool/import/ref
orchestration, append recovery, and stale-result rejection. BPR-0055 retains
core-authorized prune/cache-delete orchestration, BPR-0056 retains bounded
history paging, and BPR-0047 owns public composition. BPR-0053 retains every
target-native Windows runtime, ACL, filesystem, crash, and adversarial claim;
cross-builds are source/compile/link evidence only.
