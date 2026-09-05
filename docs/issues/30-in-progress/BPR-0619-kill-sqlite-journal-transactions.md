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
