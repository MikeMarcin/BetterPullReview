---
schema: "implement-rfc.card.v1"
id: "BPR-1382"
title: "Preserve syntax highlighting through file navigation"
status: "Ready"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P1"
---

# BPR-1382: Preserve syntax highlighting through file navigation

The owner observed syntax highlighting disappearing intermittently in the live
application, possibly after repeated Next/Previous file navigation, and asked
for direct debugging in the main checkout on 2026-10-10.

Baseline: `bee3f0dad`. Direct execution; no delegated writers. The unrelated
untracked `docs/proposals/` belongs to other work.

## Acceptance

- AC1: Reproduce and identify a concrete path that leaves a supported source
  file without its syntax classification after navigation.
- AC2: Repair that path while preserving exact source identity, bounded work,
  stale-result rejection and cached classifications.
- AC3: Add a regression that fails before the repair and passes after it;
  verify the composed application path with fixture data and offscreen output.
- AC4: Run focused existing checks and the documentation gate, and record
  evidence and the limits of reproducing the owner's live observation.

## Plan

Trace selection, classification cancellation and cached-result adoption; build
a deterministic regression; repair the proven failure; verify and commit this
slice. This does not reopen the RFC's existing palette or native acceptance.
