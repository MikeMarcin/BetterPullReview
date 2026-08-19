---
id: BPR-0353
title: Mount the capsule from a digest-bound read-only image
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0357]
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
