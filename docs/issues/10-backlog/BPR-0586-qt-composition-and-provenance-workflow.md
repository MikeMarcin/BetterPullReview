---
schema: "implement-rfc.card.v1"
id: "BPR-0586"
title: "Implement exact source selection, composition workspace, and separate draft/publish controls in Qt"
status: "Backlog"
rfcs: ["RFC-BPR-0023"]
depends_on: ["BPR-0583","BPR-0584","BPR-0585"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0023-private-review","path":"docs/rfcs/plans/rfc-bpr-0023-private-review/milestone-plan.json"}}
---

# Implement exact source selection, composition workspace, and separate draft/publish controls in Qt

## Objective

Make the private-to-draft workflow understandable, inspectable, and impossible to confuse with publication.

## Required final state

- Reviewer can select exact current/historical revisions and finding sources and inspect provenance before composition.
- Composition workspace shows source/currentness/digest status, candidate actor/lineage, and exact target.
- Reviewer-authored, exact-copy, and explicit agent-assisted modes are accessible and distinguishable.
- Stale/unplaceable/multi-target cases show actionable choices; no guessed line is offered.
- `Accept as Draft` is visibly local and separate from the existing draft editor Publish action.
- Published lineage badges link local sources to ordinary shared comments without implying GitHub stores private provenance.
- Keyboard/focus/accessibility contracts exist for selection, history, provenance details, split, accept, cancel, and publish.

## Non-goals

- A one-click squash-and-publish action
- Mandatory public AI disclosure
- AI-generated target choice

## Completion evidence

- Qt interaction tests
- Mutation canary through UI
- Rendered states for unknown/conflict/stale/multi-target
- Accessibility tree/focus-order assertions

## Documentation impact

Update the RFC-BPR-0023 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.

## Import boundary

Imported 2026-09-04 with implementation deferred at that time. Owner mandate
r1 on 2026-09-26 now authorizes implementation, plan revision and milestone
integration. The [live audit](../../verification/RFC-BPR-0023-live-audit.md)
supersedes stale baseline assumptions; required acceptance is unchanged. Follow
the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-0022-private-review-threads-pack-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.
