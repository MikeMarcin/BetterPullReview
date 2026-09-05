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
