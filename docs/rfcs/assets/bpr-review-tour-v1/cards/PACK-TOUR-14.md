---
schema: "implement-rfc.card.v1"
id: "PACK-TOUR-14"
title: "Merge website edits with local draft changes"
status: "Backlog"
rfcs: ["RFC-BPR-0042"]
depends_on: ["PACK-TOUR-13", "PACK-TOUR-07"]
---

# Merge website edits with local draft changes

Pack alias: **PACK-TOUR-14**. Allocate/remap a live BPR ID at intake; this is not an already reserved board card.

## Outcome and work

Implement baseline/local/remote comparisons, exact-span remapping after refresh, safe nonoverlapping merges and explicit conflicts. Preserve unrelated PR-template content and third-party comments. Reconstruct from a clean local profile without private provenance fabrication. Retain desired bodies through partial or denied edits and expose conditional-write limitations.

## Existing ownership seams

`Raw Markdown span mapper; revisioned drafts; profile/window fanout; provider body read/update paths`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Website edits problem/solution/narrative/footer/index, same-position heading drift, concurrent local windows, PR/account change during read/write, equal-body no-op and remote race outcomes. Verify no AI invocation during any import/merge.

Requirement ownership: `AC42-01`, `AC42-04`, `AC42-07`.

## Integration boundary

Milestone: **manual-maintenance**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
