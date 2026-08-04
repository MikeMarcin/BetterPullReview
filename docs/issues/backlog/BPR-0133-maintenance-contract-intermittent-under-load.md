---
id: BPR-0133
title: application_epoch_maintenance_public_path fails intermittently under suite load
status: Backlog
rfc: RFC-BPR-0003
depends_on: []
---

# `application_epoch_maintenance_public_path` fails intermittently under load

Observed during BPR-0130's closeout replay. The contract's final phase,
`EPOCH_MAINTENANCE_DELETED_RESTART`, failed once during a full `ctest --preset
dev` run and passed on the next full run. It passes 4 of 4 in isolation.

This is a different failure from BPR-0119, which was the diff probe sampling
derived state one tick early and is fixed. This one is in the epoch maintenance
evidence.

## Captured evidence

```text
EPOCH_MAINTENANCE_DELETED_RESTART_OK process failed (1)
PUBLIC_PATH_FAILED
EPOCH_FIXTURE_REUSE_FAILED
EPOCH_MAINTENANCE_DELETED_RESTART_FAILED
PUBLIC_PATH_STAGE:20
PUBLIC_PATH_VERDICT:presentation=1 shutdown_complete=1 cleanup_failed=0 \
  epoch_projection_failed=0 epoch_handoff_failed=0
PUBLIC_PATH_EPOCH_STATUS:Review history is inactive.  An obsolete
PUBLIC_PATH_EPOCH_CORE_STATUS:-1
PUBLIC_PATH_MAINTENANCE_EVIDENCE:0:0:0:0:0:0:0:0:1:0:0:0:1:-1:-1
GITHUB_FIXTURE_EVIDENCE:11111110:-1:10:9
GITHUB_FIXTURE_SMART_EVIDENCE:1:1:1:1
```

## What the evidence already rules out

`presentation=1` and every status flag is healthy, so none of the six conjuncts
the verdict reports is the cause. The fixture evidence is the healthy
`11111110` — the trailing zero is `SecretRetained`, which should be zero — and
the smart counts are `1:1:1:1`, which matches the single call the
`verify_deleted` probe expects. So the fixture-evidence conjunction passes too.

That leaves the maintenance-evidence conjunction, which the verdict line does
**not** cover.

## The diagnostic gap to close first

`passed` is narrowed three times: by the six facts the verdict reports, then by
the fixture-evidence conjunction, then by the maintenance-evidence conjunction.
Only the first group is reported, so a failure in the last two prints a verdict
in which everything looks fine.

Capture `passed` at each of the three points and report all three. That turns
this from "something failed" into "the maintenance conjunction failed", which
is the same move that made BPR-0119 tractable. Do this before hunting the race.

## Required direct evidence

- The verdict reports which of the three narrowing groups rejected the run.
- The failing maintenance-evidence field is named, not just the group.
- The failure is reproduced deliberately — for example by delaying whatever the
  deleted-restart phase waits for — rather than by re-running until it returns.
- Any fix is shown to fail when the behaviour it constrains is neutered, and a
  repeated full-suite loop is clean afterwards.
