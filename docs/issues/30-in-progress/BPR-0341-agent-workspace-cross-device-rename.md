---
id: BPR-0341
title: Stop the agent workspace failing across a filesystem boundary
status: In Progress
rfc: RFC-BPR-0011
depends_on: []
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

## Why no other platform shows it

`rename(2)` cannot cross a filesystem boundary; it fails with `EXDEV`. macOS
puts the per-user temporary directory and an ordinary checkout on the same
APFS volume, so the boundary never exists there and the call always succeeds.

## Required final state

- Find the rename and decide the contract: either the staging directory is
  created on the destination's filesystem, or the publication falls back to
  copy-and-replace when `EXDEV` is returned. A silent fallback must not weaken
  whatever atomicity the rename was chosen for.
- The failure is currently reported as `failed_to_start` with no cause. The
  typed outcome should distinguish a workspace that could not be published
  from an executable that could not be launched.
- The contract passes with a build-tree root and an unmodified `TMPDIR` on a
  stock Ubuntu 26.04 host.
- The macOS behavior is unchanged, and a regression test covers the crossing
  rather than relying on the host's mount layout.
