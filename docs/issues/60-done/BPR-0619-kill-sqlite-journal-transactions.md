---
schema: "implement-rfc.card.v1"
id: "BPR-0619"
title: "Kill SQLite journal transactions and recover epoch state"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0618"]
acceptance: [{"criterion":"AC1 actual precommit process death","outcome":"pass","revision":"62c62c6d1b78e61e74d29b39a51bf59fa0de4dee","evidence":{"kind":"observation","detail":"Six real receiving/spool/install SQL transaction kills, with and without prior history, passed rollback/integrity/foreign-key checks in epoch_transaction_kill_recovery (165.17 s)."}},{"criterion":"AC2 normal restart and exact history","outcome":"pass","revision":"62c62c6d1b78e61e74d29b39a51bf59fa0de4dee","evidence":{"kind":"observation","detail":"All six courses reached ready twice through the real host/Git/SQLite composition, retained exactly one epoch/ref pair, passed Git fsck and preserved prior committed identity. Earlier four epoch kill courses also passed (107.98 s)."}},{"criterion":"AC3 bounded ownership-preserving recovery","outcome":"pass","revision":"62c62c6d1b78e61e74d29b39a51bf59fa0de4dee","evidence":{"kind":"observation","detail":"git_cache_contract passed (219.26 s), including substituted ownership and payload refusal, retained unowned artifacts, replay and 4096-entry admission refusal/recovery; transaction oracle independently checks unowned native file identity across restarts."}},{"criterion":"AC4 release seam and focused contracts","outcome":"pass","revision":"62c62c6d1b78e61e74d29b39a51bf59fa0de4dee","evidence":{"kind":"observation","detail":"Dev and tests-off Git/SQLite adapter targets built; nm -C confirmed journal checkpoint absent from app-release. application_epoch_host and sqlite_epoch_catalog_behavior_contract passed 2/2 (41.34 s)."}},{"criterion":"AC5 integration and documentation","outcome":"pass","revision":"62c62c6d1b78e61e74d29b39a51bf59fa0de4dee","evidence":{"kind":"observation","detail":"Architecture policy updated; network isolation manifest, documentation gate, 62 documentation tests, focused clang-format and git diff --check passed. Source reviewed for ownership, cancellation and bounded work. All verification local; no GitHub Actions, push, paid CI or credentials."}}]
---

# Kill SQLite journal transactions and recover epoch state

RFC-0013's recovery matrix names interruption during a DB write. BPR-0618
covers completed host-operation boundaries; its SQLite transactions have
already returned at those checkpoints. Add actual forced-death evidence while
a journal transaction has made its SQL writes but has not committed.

## Acceptance and scope

- Extend the real epoch host/Git/SQLite process-kill fixture with receiving,
  completed-spool and installation journal transaction checkpoints before
  COMMIT. Reuse the existing bounded kill/reap helper and contained roots.
- Use a narrowly scoped test-only checkpoint in the existing SQLite testing
  boundary if no existing callback can observe the live transaction. The
  selected production record methods must execute the actual writes; do not
  replace persistence with an in-memory store or kill a parallel SQL writer.
- A fresh production host must recover to ready without duplicated history,
  ref pairs or mutations. Previously committed epoch identity must survive;
  uncommitted journal facts must not be reported as committed.
- Independently verify SQLite integrity/foreign keys, Git fsck/refs and exact
  owned-artifact cleanup. Preserve substituted, preexisting or unrelated
  files. Any startup failure exposed at these boundaries belongs in this card.
- Keep test checkpoint control absent with BUILD_TESTING=OFF. Verify the
  focused fixture and affected direct adapter/host contracts; add a tests-off
  compile check if the implementation introduces a new testing seam.

## Known boundary to investigate

The receiving file is created before its exact identity can be persisted.
Killing the receiving transaction before commit can therefore leave an empty
file without a durable ownership fact. The current recovery contract refuses
filename-only deletion. Reproduce this through the actual transaction before
choosing a repair; preserve the exact-ownership rule and account for any
necessary protocol change explicitly.

## Ownership and verification

Own `src/adapters/sqlite/include/bpr/adapters/sqlite/testing.hpp`, the concrete
catalog transaction implementation and test-only registration, the existing
real epoch-maintenance fixture and CMake registration, and a narrow transaction
course in the process-kill runner (or a separate runner reusing its helper).
Any necessary production repair stays in the application epoch/cache/catalog
contracts and their concrete owners; record it here before expanding checks.
Update the owning architecture paragraph and BPR-0617's criterion record.

Run an exact network-isolated transaction-kill selector, affected focused
contracts and one real persisted-state path. Use the Mac only. No production
GUI, credentials, sign-out, GitHub Actions, workflow dispatch, push, remote
mutation, paid CI or broad test matrix is authorized by this card.

## Transaction regression evidence (2026-09-05)

The new `epoch_transaction_kill_recovery` selector kills the selected real
SQLite record methods after their writes and output validation, before COMMIT.
It runs receiving/spool/install both on an empty catalog and after a committed
epoch, then requests ordinary host recovery and a second restart. The history
courses use a new accepted base branch and snapshot on the same real host;
read-only SQL independently checks rollback and the prior epoch identity.

At `683cb32c` plus this slice, the local selector failed in 106.78 seconds:
all six rollback/integrity/foreign-key checks passed; spool, spool-history
and install-history recovered with exact refs, Git fsck and repeated restart.
Receiving, receiving-history and first-install recovery failed with
`repair_required`. The history receiving case retains the committed epoch
identity in SQL, but ordinary startup remains unavailable. These are production
recovery failures, not accepted outcomes or RFC closeout evidence.

The test hook is compiled only under the existing `BPR_SQLITE_TESTING` boundary.
The focused dev target and tests-off SQLite adapter built successfully; `nm -C`
confirmed the hook present in dev and absent in app-release. The network
isolation manifest passed in 3.72 seconds. No test used a network allowance,
production application, credential or remote action. All killed children were
reaped and the parent-owned temporary roots were removed.

## Recovery repair

The cache now distinguishes two observations from exact durable ownership:

- `inventory_only`: the durable spool receipt describes the complete prior
  inventory plus exactly one new pack/index pair of the expected pack size.
  Abort requires absent operation refs and exact accepted object closure,
  leaves installed files intact and exact-cleans only the owned spool. This
  does not authorize publication or deletion of an unrecorded installed file.
- `unowned_empty`: a private, regular, zero-byte receiving file has no committed
  receiving/spool/install fact and no completed spool. With no operation refs,
  abort preserves it and abandons the interrupted operation. The file is not
  read as payload, reused or deleted. The fresh-host oracle verifies its native
  device/inode identity and zero size survive both restarts.

Incoming admission counts all directory entries and refuses at the existing
4096-entry pack capacity, without removing anything to make room. The scan is
bounded and cancellable. Tests cover refusal at capacity and admission after
the fixture itself removes its canaries. Unknown nonempty payload, substituted
files against committed ownership, unexpected installed inventory and
inventory-only staging refs continue to refuse recovery.

This is an explicit availability-policy change for unowned empty artifacts,
not relaxed deletion authority. Such leftovers remain on disk; reaching the
admission limit requires cleanup or repair instead of automatic deletion.
Previously terminal `repair_required` catalog records are not silently reopened.
The owning protocol is documented in `docs/architecture/git-and-epoch-model.md`.

## Final acceptance

Source repair: `62c62c6d1b78e61e74d29b39a51bf59fa0de4dee`. Transaction regression: `42ef8536`.

- `epoch_transaction_kill_recovery`: all six precommit forced-death courses
  pass (165.17 s), including independent SQL rollback, integrity/foreign keys,
  prior committed identity, two ordinary fresh-host recoveries, Git fsck,
  one exact ref pair and preserved unowned empty file identity.
- `epoch_kill_recovery`: all four earlier fetch/spool/prepared/visible courses
  pass (107.98 s); their owned-spool cleanup requirement is unchanged.
- `git_cache_contract`: pass (219.26 s), including inventory-only refusal with
  refs or unrelated packs, preserved installed files on replay, exact ownership
  substitution refusal, unowned payload refusal, empty-file preservation and
  incoming-capacity refusal/recovery. The three suites passed together in
  327.25 seconds using local network-isolated fixtures.
- `application_epoch_host` and `sqlite_epoch_catalog_behavior_contract`: 2/2
  pass (0.54 s / 40.80 s). Dev targets and tests-off Git/SQLite adapter targets
  build. Symbol inspection again confirms the SQLite checkpoint absent from
  app-release and present only in dev.
- Network-isolation manifest, documentation gate, 62 documentation tests,
  focused clang-format and `git diff --check` pass. The changed code was
  reviewed for ownership, bounded admission, cancellation and readable policy.

All card criteria are satisfied on the Mac. This is process-death recovery
evidence, not power-loss durability, death inside index-pack, AI-session kill
coverage or Windows/Linux target-native execution. No production GUI, credential,
Sign out, GitHub request, Actions job, workflow dispatch, push or paid CI was
used. Fixture roots and children were cleaned up; the existing build trees
remain.
