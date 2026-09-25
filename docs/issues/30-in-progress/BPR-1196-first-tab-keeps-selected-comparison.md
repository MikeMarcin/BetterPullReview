---
schema: "implement-rfc.card.v1"
id: "BPR-1196"
title: "First tab keeps the selected comparison"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"origin":"RFC-BPR-0052/BPR-1132 integration regression", "mandate":"owner-20260925-unattended-parallel/cache-identity"}
---

# First tab keeps the selected comparison

The full Mac run at `f6de40b6` timed out in
`application_large_review_cache_bytes` after selecting the first file. The
course accepted epoch 1 after retaining 32 heads, but the selected source did
not satisfy epoch 1 identity before the unchanged 840-second run budget.

## Acceptance

- AC1: A public comparison selection before any tab opens remains the exact
  accepted, window, tab and displayed comparison when the first file opens.
  A focused public course exposes the regression before the product repair.
- AC2: The unchanged cache-byte pressure course passes with exact source,
  eviction, supersession and cleanup evidence; the 840/900-second bounds stay
  unchanged.
- AC3: Focused document-tab and epoch checks, composition source pins and the
  documentation gate pass on the candidate. Required owner/native decisions
  on [[BPR-1135]] remain pending; Windows/Linux runs remain deferred.

## Execution plan

Adopt RFC-0052 m2's authored comparison/tab identity contract as a follow-up
regression slice. Source baseline `f6de40b633ea8b879d3643ad5a259169fd976975`;
isolated branch `codex/bpr1132-cache-identity`. Diagnose all four identities at
selection, repair only the comparison/tab composition boundary, prove a
focused failure control and run the complete cache-byte course, then review,
commit explicit paths and release the writer to the coordinator. The primary
checkout and frozen attended app are outside this writer's scope.

## Diagnosis and candidate

The short two-head public course failed at `f6de40b6` plus the passive evidence
sampler: before opening the file, accepted/window/tab were `1:1 / 2:2 / none`;
after opening, accepted/window/tab/displayed were all `2:2`. It failed in
29.74 seconds with the exact-pair assertion. Updating the empty-tab observer
alone still failed in 28.84 seconds: epoch acceptance had queued its observation
behind the next file gesture. The corrected composition also adopts that
observation synchronously before first-tab creation. The focused course then
passed in 27.35 seconds, rendered the full Read window and drained shutdown.

The product change is confined to the empty-tab observer and first-file
composition; existing active-tab rules and the core's immutable pair identities
are unchanged. The sampler is test-build-only and reads state without driving
it. The full application and production-manifest pins are resealed after this
source review; the mutation-branch digest is unchanged. The architecture and
workbench specification now describe comparison ownership with no tab open.

Full cache-byte, existing tab/epoch integration and final upkeep evidence are
pending at this implementation checkpoint. Logs and generated renders remain
under `/Users/jaeger/.codex/tmp/bpr1196-*` and this worktree's `build/dev`.
