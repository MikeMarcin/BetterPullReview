---
id: BPR-0072
title: Implement canonical diff and alignment records
status: In Progress
rfc: RFC-BPR-0004
depends_on: [BPR-0071]
---

# Implement canonical diff and alignment records

Add the Qt/provider/effect-free comparison identity, normalized option digest,
half-open line spans, line/boundary references, ordered alignment segments,
line pairs, change groups, move relations, diagnostics, and stable identities.
Validate exact left/right coverage, bounds, canonical adjacency, monotonicity,
kind/span consistency, and nonempty combined extent.

Use compact immutable publication values and deterministic IDs derived only
from exact comparison inputs and canonical structure. Do not include measured
pixels, Qt indexes, paths-as-authority, worker completion order, or live scroll
state.

## Required direct evidence

- Table and generated/property cases cover empty files, equal text, insertions,
  deletions, replacements, invalid spans, overflow/bounds, and deterministic
  replay.
- A public validator rejects every malformed invariant and accepts canonical
  empty-side segments without inventing source lines.
- Core-only/dependency, sanitizer, documentation, workflow-absence, and diff
  hygiene gates pass.
