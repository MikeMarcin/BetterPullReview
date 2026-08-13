---
id: BPR-0288
title: Implement deterministic Dif asset tooling
status: Done
rfc: RFC-BPR-0019
depends_on: [BPR-0286]
---

# Implement deterministic Dif asset tooling

Implement the offline, noninteractive contract for validating owned source
frames, producing runtime strips and a fixed descriptor, and rendering review
previews. Use only declared, reproducible dependencies and no production art.

## Required implementation

- Define one versioned manifest/provenance schema and strict path allowlist.
- Parse and emit bounded PNG data deterministically, rejecting unsafe or
  undeclared chunks, modes, dimensions, metadata, and decompressed sizes.
- Implement the fixed resize/color/alpha contract, transparent gutters,
  manifest-order strip packing, descriptor generation, and digest inventory.
- Validate the exact three-clip graph, geometry, timing, rate, frame, decoded
  pixel, file-byte, seam, and handoff limits.
- Render deterministic contact sheets, overlays, context plates, and animated
  previews without making preview output part of the runtime product pack.

## Completion evidence

- Focused unit tests force each material validator failure and restore it.
- Two empty-output executions with a synthetic source pack agree byte for byte.
- No ordinary CMake target invokes the tool or requires its dependencies.
- Tool diagnostics contain stable asset-relative identities only, never a
  private source path, prompt, credential, or image metadata payload.

## Result

- Commit `1a5c9b8` adds one Python-standard-library CLI with `validate`,
  `generate`, `verify`, and `preview` commands; no CMake target invokes it.
- Canonical JSON, exact supplied-reference hashes, safe relative paths, early
  symlink rejection, controlled RGBA PNG decoding/encoding, zero hidden RGB,
  fixed box resampling, exact clip/return/geometry/timing/budget rules, strip
  gutters, generated catalog/digests, and byte-tree comparison fail closed.
- Deterministic preview output includes 1x contact sheets, registration
  overlays, declared-speed APNGs at 64/72/96 pixels, calm-seam and greet-handoff
  courses, appearance plates, and a reduced-motion comparison. Preview media
  remains outside the runtime pack and supplies no attended verdict.
- `python3 tests/scripts/dif_asset_tests.py` passes 18/18 non-vacuous positive
  and hostile cases. Registered `dif_assets_contract` passes in both `dev` and
  `core-only` CTest at 8.63 and 8.66 seconds respectively.
- An independent `rfc-scout` compiled the generated synthetic catalog with
  clang++, checked APNG sequencing, found three blockers, and verified their
  repairs: portable catalog text, one shared neutral body box, and no source
  symlink traversal before digest reads. Its final focused replay passed 18/18
  with no remaining blocker.
- No production Dif source, generated runtime strip, Qt resource, application
  composition, art dependency, model call, or network dependency was added.
