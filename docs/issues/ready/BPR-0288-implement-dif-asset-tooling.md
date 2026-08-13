---
id: BPR-0288
title: Implement deterministic Dif asset tooling
status: Ready
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
