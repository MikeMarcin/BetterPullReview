---
schema: "implement-rfc.card.v1"
id: "BPR-0308"
title: "Document anonymous public-preview behavior and boundaries"
status: "Done"
rfcs: ["RFC-BPR-0017"]
depends_on: ["BPR-0305","BPR-0306","BPR-0307","BPR-0558"]
---

# Document anonymous public-preview behavior and boundaries

## Required final state

- Product, system, GitHub host/provider, workbench/queue/epoch, security,
  build/test, operator, and verification documentation states the exact public
  preview, limits, unavailable observations/actions, 404 rule, and GHES refusal.
- The ledger classifies every changed path from the immutable base and records
  exact updates or evidence-backed no-impact dispositions.
- Documentation never calls the mode authenticated, full review, triage, or
  private-capable and never treats official service documentation as executed
  probe evidence.
- Agent guidance changes only for a stable reusable rule; documentation gates
  and focused negatives pass.

## Claimed — 2026-09-04

Dependencies are terminal. Update current behavior from `a319dd51` and
`dd822350`, preserve historical attended evidence unchanged, and classify
the immutable-base paths in the ledger this card explicitly requires.
No product behavior changes or new live requests belong to this slice.

## Product-entry gap found — 2026-09-04

The source-backed documentation pass found no ordinary UI caller for preview
activation. [[BPR-0558]] now owns its bounded repair and is a required dependency.
The draft documentation remains uncommitted until that gesture is verified.

## Completed — 2026-09-04

Updated current product, provider/host, cache/epoch/queue, security, UX, operator,
build/test, fixture, master-matrix, and release documentation, with the exact
Mac-enabled/Linux-and-Windows-unavailable policy. The original RFC findings
are labeled historical at `RFC_BASE_SHA`; REST comment groups and explicit
unavailable viewer/thread/action facts now describe the implementation.
The Qt guide adds the narrow preview-projection rule only. BPR-0558's ordinary
control repair and Release probes establish the documented discoverability.

`docs/verification/RFC-BPR-0017-path-dispositions.tsv` classifies every exact
changed path from `a45750f6bd84716240aa27a20f0734f33808ab8e`, expanding renames
and retaining commit/owner support. Historical review bytes and source-only
plan snapshots were not rewritten. The RFC ledger reconciles terminal
GraphQL-probe and Windows-unavailability evidence without claiming new execution.

Documentation structure and its 54 positive/negative tests pass, as does
`git diff --check`. Product inputs are unchanged from BPR-0558, so its 7/7
integrator reproduction, worker's 12 focused tests and Release/composition
evidence remain valid. RFC-0017 remains Proposed pending BPR-0309 closeout.
No temporary resources were created for documentation.
