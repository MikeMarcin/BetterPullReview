---
id: BPR-0449
title: Run the designated live lane from the read-only image
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0448]
---

# Run the designated live lane from the read-only image

BPR-0256's v005 capability-critical review falsified the runtime half of
BPR-0353's read-only topology. `stage-zero` mounts the digest-bound image,
verifies it, and detaches immediately. It never starts stage one. The opt-in
CTest registration instead launches the writable plain-directory supervisor,
so the read-only mount protects neither the supervisor/application lifetime nor
lazy nested-resource loads after `SIGCONT`.

The v005 packet disclosed the plain-directory launch, but packet prose cannot
waive BPR-0353's required runtime property or BPR-0354's explicit reliance on
the mount after resume.

## Required final state

- The exact digest-bound image is attached read-only beneath a fresh
  owner-private mount root, and the registered live lane launches stage one
  only from that mount.
- The mounted inventory, verifier, supervisor, application, and nested runtime
  resources remain under that same mount until the complete child lane exits.
- Writable mount, image digest drift, mount identity drift, early detach, or a
  detach that would require force refuses the lane.
- The opt-in manifest no longer names a supervisor or inventory from the plain
  build-tree capsule. Its sole command is the exact bounded stage-zero runtime
  topology, and the default graph still contains zero live entries.
- Mocked and real-image contracts prove mount → verify → supervisor/app run →
  ordinary detach ordering, including failure cleanup without force.

## Non-goals

- No live GitHub call, credential access, approval materialization, or remote
  mutation.
- No claim that DMG generation is byte-reproducible. A successor binds the
  exact stored image it reviews.
- No v006 operator packet until this card and BPR-0448 are complete and the
  entire tests-disabled Release capsule is regenerated.
