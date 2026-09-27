---
schema: "implement-rfc.card.v1"
id: "BPR-0996"
title: "Implement the ordinary Markdown tour codec and safe source mapping"
status: "Ready"
rfcs: ["RFC-BPR-0040"]
depends_on: ["BPR-0992", "BPR-0995"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-06","milestone":"github-round-trip","acceptance":["AC40-01","AC40-02","AC40-04","AC40-05"],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-0996: Implement the ordinary Markdown tour codec and safe source mapping

Pack alias: **PACK-TOUR-06** (live `BPR-0996`, allocated at import).

## Outcome and work

Implement generated description sections, review-summary index and ordinary stop heading/footer forms. Build safe Markdown labels and canonical links. Parse selected top-level sections/lists/footers without touching quoted/fenced/raw HTML content; preserve untouched bytes and permit manual source-span mapping. Keep RFC-0022 fallback permalink on the first line. Do not require hidden markers.

## Existing ownership seams

`Existing Markdown source/navigation policy; src/adapters/github normalized comment models; new bounded pure codec`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

Golden corpus for fences, CRLF, duplicate headings/titles, nested lists, disclosures, escaped labels, custom website edits, unrecognized footer prose and raw-source exact round trips.

Requirement ownership: `AC40-01`, `AC40-02`, `AC40-04`, `AC40-05`.

## Integration boundary

Milestone: **github-round-trip**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
