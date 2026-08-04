---
id: BPR-0152
title: Simplification and closeout preparation for RFC-BPR-0009
status: Backlog
rfc: RFC-BPR-0009
depends_on: [BPR-0151]
---

# Simplification and closeout preparation for RFC-BPR-0009

See the RFC-BPR-0009 verification plan for this slice's place in the sequence.

The rule this RFC exists to protect: **a nearby code change is evidence, not a
verdict.** Evidence reaches `needs verification` and never `verified`. Only an
explicit reviewer intent verifies, and this slice must not open a path around
that however convenient it would be.

## Required direct evidence

- The behaviour this slice owns is exercised directly, not inferred from a
  neighbouring slice passing.
- No path through this layer reaches `verified` without the explicit reviewer
  intent.
- Every refusal carries its own typed reason.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
