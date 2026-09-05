---
schema: "implement-rfc.card.v1"
id: "BPR-0619"
title: "Kill SQLite journal transactions and recover epoch state"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0618"]
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

Next: repair first-install recovery without granting deletion authority over
an unjournaled pack, and resolve the unowned empty receiving-file policy.
Installed artifacts are already preserved by ordinary operation abort; an
unrecorded installation must remain distinct from exact journaled ownership.
The receiving-file case must continue to preserve unknown/substituted files.
