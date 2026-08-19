---
id: BPR-0353
title: Mount the capsule from a digest-bound read-only image
status: Done
rfc: RFC-BPR-0008
depends_on: [BPR-0357]
completed: 2026-08-18
---

# Mount the capsule from a digest-bound read-only image

[[BPR-0262]] fact_1: an external stage-zero launcher mounts a digest-bound disk
image read-only beneath a fresh owner-private mount root, and the app, stage-one
supervisor, verifier, and inventory all live inside that image.

None of this exists. `hdiutil`, `UDZO`, `APFS`, `attach`, `detach`, and the
read-only mount appear nowhere in `scripts/`, `src/`, `cmake/`, or `tests/`; the
only trace is the operator packet's prose record of a manual host probe.
`scripts/designated_live_macos_capsule.py:530` builds a plain directory tree
with `output.mkdir(mode=0o700)`.

The packet also records two limits the design must respect: the tested
`/dev/fd/N` image path is not accepted by DiskImages, so no FD-atomic
association with the pre-attach pathname may be claimed, and
`TASK_RESOURCE_NOTIFY_PORT` is unusable for the handoff.

## Required final state

- A stage-zero launcher attaches the image `-readonly` beneath a fresh
  owner-private mount root and detaches it without force.
- The image is digest-bound, and its contents are the app, stage one, the
  verifier, and the inventory.
- A writable mount, a detach that requires force, or mount drift fails closed
  (fact_6).
- `scripts/designated_live_macos_capsule.py` produces the image, and
  `designated_live_macos_capsule_contract` covers the new output.

## Result — 2026-08-18

The capsule now ships as a digest-bound read-only image, and a stage-zero
launcher mounts it beneath a fresh owner-private root and detaches it without
force.

`scripts/designated_live_macos_capsule.py` gains two subcommands. `image`
stages the capsule and its inventory side by side, writes a UDZO image, and
reports the digest that binds it. `stage-zero` checks that digest, creates the
mount root, attaches read-only, requires the mount to refuse writes, verifies
the inventory from inside the mount, and detaches — in a `finally`, so a
failure anywhere in the middle still detaches.

### Where the inventory goes, and why it is beside the capsule rather than in it

fact_1 puts the app, stage one, the verifier and the inventory inside the
image. It cannot put the inventory inside the *capsule*: the inventory records
a digest for every entry it walks, so an inventory inside its own subtree would
have to cover itself. The image therefore holds two siblings —
`capsule/` and `capsule.inventory` — and the image digest is what binds them
together. That also answers the question [[BPR-0352]] ran into from the other
side, where the application could not compute the inventory digest itself.

### fact_5's limit, respected by not claiming anything

`image_sha256` is a hash of a file at rest. The operator packet records that
the tested `/dev/fd/N` path is not accepted by DiskImages, so there is no way
to tie the bytes hashed to the bytes attached without a window. The function
says so where it is defined. No FD-atomic attach identity is claimed, because
none is available.

### Evidence

`designated_live_macos_capsule_contract` grew six cases, and they divide on
purpose:

- three mock `command` and assert the argv the launcher builds — that attach
  asks for `-readonly` and `-nobrowse`, that detach never passes `-force` and
  refuses when a plain detach fails, and that a drifted image digest refuses
  before anything is attached at all;
- two exercise real filesystem behaviour — a writable mount is refused and the
  probe it wrote is removed, and mount roots are fresh, distinct and `0700`;
- one makes a real image, attaches it, reads the app and the inventory out of
  the mount, tries to write to it, and detaches, then confirms `hdiutil info`
  no longer lists it.

The last one is the reason the others are not sufficient. A mocked read-only
mount proves that the string `-readonly` was typed.

`hdiutil` was confirmed to work under the repository's network-isolation
sandbox before any of this was designed, since a contract that cannot run in
the default graph would have needed a different shape.

### Assertions proven by mutation

| Mutation | Result |
|---|---|
| attach without `-readonly` | fails "attach asks for a read-only unbrowsable mount" |
| detach with `-force` | fails "detach never asks for force and refuses when it would need it" |

Worth recording precisely, because it bounds what the first row proves:
removing `-readonly` did **not** fail the real round trip. A UDZO image is
read-only by format, so the mount refuses writes either way. The flag is
belt-and-braces over that, and the argv assertion is what actually pins it. A
future change to a writable image format would be caught by the argv test and
by `require_read_only_mount`, not by the format's accident.

### What is deliberately not wired

The capsule CMake target still builds the directory tree and does not invoke
`image`. That graph requires `BUILD_TESTING=OFF` plus `macdeployqt` and a
resolvable Qt, and it cannot be built here — adding an unexercised step to it
would be the same never-compiled-code trap [[BPR-0240]] recorded against the
Windows translation unit. The script produces the image and the contract covers
it; wiring the release graph belongs with whoever can build that graph.

Zero disk images were left attached: checked before starting, after every
course, and at the end.
