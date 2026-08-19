---
id: BPR-0354
title: Close the nested resource check/use window
status: Done
rfc: RFC-BPR-0008
depends_on: [BPR-0353]
completed: 2026-08-18
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

## Result — 2026-08-18

The window is closed for the interval the supervisor controls.
`designated_live_macos_capsule_identity` captures every entry beneath a
retained capsule root immediately after the Python verifier succeeds, and
revalidates immediately before `SIGCONT`. A nested dylib, framework, plugin or
`qt.conf` substituted in between refuses the launch, before the child resumes.

Everything is asked through a descriptor opened once, `O_NOFOLLOW`, and kept.
Re-resolving a path from its text is a second resolution, and a second
resolution can be answered by a directory other than the one the verifier
walked — which is the whole mechanism the card is about.

### What it records, and why identity as well as content

Each entry carries device, inode, size, modification time to the nanosecond,
mode, and either a SHA-256 or a validated symlink target. Content alone would
miss a file replaced with identical bytes at a new inode, and the contract
proves that case specifically rather than assuming it.

Refusals rather than descriptions: an escaping symlink, a kind that is neither
file, directory nor symlink, an unreadable entry, or a tree past its bounds all
produce nothing instead of a partial answer. A partial answer here is worse
than none, because the caller's next move is to trust it.

### The barrier is a function call

`macos_capsule_identity` captures, mutates one nested entry, and revalidates.
No sleeping, no spawning, no retrying — the card asks for a deterministic and
hermetic barrier and says outright that a timing-dependent reproduction is not
evidence. Four substitutions are covered: changed content, identical bytes at a
new inode, an entry added, an entry removed.

Mutation-checked by stopping the walk from recursing. All five nested
assertions fail, including "the capture reaches nested entries rather than the
top level only" — which is the assertion that would otherwise let a top-level
only check pass as though it had covered the tree.

### Both sides require the mount independently

fact_2 asks for that, and it is now true in two places rather than asserted in
one: [[BPR-0353]]'s `stage_zero` verifies the inventory from inside the mount
before handing over, and this component re-verifies the tree from the retained
descriptor before the resume. Neither is trusted to have checked on the other's
behalf.

### An honest limit on the evidence

`designated_live_macos_supervisor.cpp` compiles only in the capsule graph,
which needs `BUILD_TESTING=OFF` plus `macdeployqt` and a resolvable Qt, and
that graph cannot be built here. So the supervisor edits from [[BPR-0351]],
[[BPR-0352]] and this card have **not** been compiled by a real target build.
They do type-check: `clang++ -fsyntax-only -std=c++23` over the file with the
project's include paths resolves every symbol, including the new capsule
identity and provenance calls, and reports nothing.

That is meaningfully weaker than a build. It is recorded rather than glossed
because [[BPR-0240]] lost time to exactly this shape — a translation unit that
looked finished and had never been compiled. What *is* compiled and executed is
the component the supervisor calls, which is where the logic lives.

The interval after `SIGCONT` is not covered here and is not meant to be: that
is the read-only mount from [[BPR-0353]]. The two hold together.
