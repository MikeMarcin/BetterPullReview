---
provisional_id: PACK-BPR-0022-11
title: Implement exact source selection, composition workspace, and separate draft/publish controls in Qt
status: Backlog
rfc: RFC-BPR-0022
milestone: Post-Pack v2
live_id: allocate-on-import
depends_on: [PACK-BPR-0022-08, PACK-BPR-0022-09, PACK-BPR-0022-10]
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

Update the RFC-BPR-0022 documentation ledger with actual changed paths,
operator-visible behavior, limits, commands, and evidence, or record a reviewed
no-impact result for this slice.
