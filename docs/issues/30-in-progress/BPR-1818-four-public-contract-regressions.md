---
schema: "implement-rfc.card.v1"
id: "BPR-1818"
title: "Restore four native review public contracts"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-1817"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "release-v041-same-preview-waivers-r1", "external_state": "/Users/jaeger/.codex/release-runs/v0.4.1-20261029", "milestone_plan": {"id": "release-v041", "path": "docs/rfcs/plans/release-v041/milestone-plan.json"}}
---

# Restore four native review public contracts

Frozen build-4 Mac Release qualification has four additional functional failures,
with no expired timing budget in either composed-contract failure. Preserve each
raw result and distinguish actual changed behavior from stale test assumptions
using the published 0.4.0 producer and its explicit repair history. Work only in
the isolated repair checkout while native build-4 suites retain their inputs.

## Acceptance

- AC1: Diagnose and correct the review-behaviour continuation contract reporting
  `continue=review:filter=unread:honoured=0`. Preserve the approved continuation
  and unread-filter semantics, with a meaningful focused public-path control.
- AC2: Diagnose and correct the reviewer-request contract's repeated-request
  refusal after a successful request. Preserve idempotence and current approved
  behavior, with a repeated-action negative control.
- AC3: Diagnose and correct Markdown image restart handling reporting that
  resolution or fetching occurred after restart. Preserve the approved cache,
  privacy and capability boundary; test the restart and missing/invalid state.
- AC4: Diagnose and correct the thread-word public contract. The empty-filter
  clear button's accessible descriptions currently call the whole review thread
  a conversation. Preserve consistent approved vocabulary and accessible output.
- AC5: Record exact historical/source comparisons and focused native public
  proofs, unchanged-functional evidence bridges and any new real input rebuilds.
  Retain original build-4 failures and capacity interruption without pass credit.
  Do not relabel frozen producers; changed source requires a successor cohort.

## Scope and authority

The root coordinator reserved BPR-1818 after the actual four failures. There is
no timing waiver, generic historical waiver, new review gate or attended-review
requirement for restoring these existing contracts. Load the required desktop
frontend-design skill before Qt, accessibility or UX edits. Keep semantic fixes
narrow, use meaningful negative controls, and preserve source/Git/artifacts/maps
and actual source metadata on every existing producer or reused test graph.
