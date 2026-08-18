---
id: BPR-0354
title: Close the nested resource check/use window
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0353]
---

# Close the nested resource check/use window

[[BPR-0262]] fact_2: stage one and the app independently require the same
read-only mount, retained no-link descriptors, and exact inventory and signature
identities, and fail closed on nested-resource substitution or mount drift.

The window is real and was verified against the live tree.
`run_static_verifier` (`src/app/src/designated_live_macos_supervisor.cpp:365-384`)
spawns `/usr/bin/python3` and blocks in `wait_success` (`:312-323`) until that
process has fully exited. Only then does `launch_attended_application` run, and
its only post-verification checks are `exact_file_identity` (`:386-402`) and
`guest_signing_digest` (`:277-310`) — both scoped to the top-level app
executable. Nothing re-checks a nested dylib, framework, plugin, or `qt.conf`
between the verifier's exit and `SIGCONT`.

The recursive walk itself is sound: `inventory_entries`
(`scripts/designated_live_macos_capsule.py:78-105`) records kind, mode, and
either a sha256 or a validated symlink target for every entry, and
`verify_inventory` (`:470-499`) recomputes and requires exact equality. It just
runs in a process that is gone before the child starts.

## Required final state

- Substituting a nested capsule entry at an injected post-verification barrier
  refuses launch before GUI resume.
- Retained no-link descriptors, not re-resolved paths, are what the identity is
  checked against.
- Both stage one and the app require the mount independently, so neither is
  trusted to have checked on the other's behalf.
- The barrier is deterministic and hermetic; a timing-dependent reproduction is
  not evidence.
