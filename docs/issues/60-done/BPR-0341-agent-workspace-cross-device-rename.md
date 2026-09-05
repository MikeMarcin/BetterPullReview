---
schema: "implement-rfc.card.v1"
id: "BPR-0341"
title: "Stop the agent workspace failing across a filesystem boundary"
status: "Done"
rfcs: ["RFC-BPR-0011"]
depends_on: []
completed: "2026-08-17"
---

# Stop the agent workspace failing across a filesystem boundary

The agent invocation path fails whenever the system temporary directory and
the agent workspace sit on different filesystems. This is product behavior,
not a test artifact, and it is the default arrangement on a stock Ubuntu
26.04: systemd mounts `/tmp` as `tmpfs`, while a checkout and its build tree
are on the root filesystem.

## Reproduction

`application_agent_invocation_composition_contract` fails with the build-tree
root it is registered with, three runs out of three, and passes three out of
three when `TMPDIR` is moved onto the build filesystem. The 2x2 below is from
direct binary runs and does not involve the test sandbox, which was ruled out
by running the same binary with and without it:

| workspace root | `TMPDIR` | same filesystem | result |
|---|---|---|---|
| `/tmp` (tmpfs) | `/tmp` (tmpfs) | yes | passes |
| ext4 | ext4 | yes | passes |
| ext4 | `/tmp` (tmpfs) | no | `failed_to_start` |
| `/tmp` (tmpfs) | ext4 | no | `failed_to_start` |

```sh
TMPDIR=/some/path/on/the/build/filesystem \
  ctest --test-dir build/dev \
  -R '^application_agent_invocation_composition_contract$'
```

It passes with that variable set and fails without it. Nothing else changes.

`application_epoch_maintenance_public_path` was briefly attributed here and is
not: it alternates pass and fail across consecutive runs whatever `TMPDIR` is,
which is a different defect. BPR-0345 carries it.

## Mechanism — corrected 2026-08-17

This card first asserted a `rename(2)` returning `EXDEV` in our own code. That
was inference from the 2x2, not a measurement, and it is wrong: there is no
such rename on this path. `materialize_agent_workspace` performs no rename
between the two roots, and its precondition block is not the branch that
rejects.

Instrumenting the real path gives the actual answer:

```
BPRDBG proc-materialize-fail=11        # git_process_failure::nonzero_exit
BPRDBG materialize-fail=1              # agent_workspace_failure::unavailable
```

A **git subprocess exits nonzero** during materialization, and the host maps
that to `unavailable`, which `workspace_outcome` turns into
`failed_to_start`. So the reported outcome names the launch, while the failure
is in the workspace's git work several layers earlier.

What is still established is the dependency itself: the workspace parent and
the retained cache must be on the same filesystem. Both roots on tmpfs passes
and both on ext4 passes, so it is the relationship rather than either
filesystem. macOS keeps the per-user temporary directory and an ordinary
checkout on one APFS volume, so the relationship always holds there.

The remaining work is to capture that subprocess's standard error and name the
git operation, rather than to assume which one it is.

## Required final state

- Identify the git operation that exits nonzero and why the filesystem
  relationship decides it, from its captured standard error rather than by
  inference.
- Repair it at that layer. If the answer is that the workspace must share a
  filesystem with the cache, the composition should place it there rather than
  taking `QDir::tempPath()` and hoping.
- The failure is currently reported as `failed_to_start` with no cause, three
  layers from where it happened. A workspace that could not be materialized
  should not be indistinguishable from an executable that could not be
  launched, and the subprocess's error should reach the evidence.
- The contract passes with a build-tree root and an unmodified `TMPDIR` on a
  stock Ubuntu 26.04 host.
- The macOS behavior is unchanged, and a regression test covers the crossing
  rather than relying on the host's mount layout.
