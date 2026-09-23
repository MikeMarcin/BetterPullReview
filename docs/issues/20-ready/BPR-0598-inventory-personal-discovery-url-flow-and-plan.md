---
schema: "implement-rfc.card.v1"
id: "BPR-0598"
title: "Inventory personal discovery and URL flow and plan RFC-BPR-0026"
status: "Ready"
rfcs: ["RFC-BPR-0026"]
depends_on: ["BPR-0597","BPR-0244"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
---

# Inventory personal discovery and URL flow and plan RFC-BPR-0026

## Objective

Trace repository search in PR loading, current queue observations/membership, viewer identity, provider capability contracts, pending setup, and workspace opening. Define bounded personal discovery and exact URL-to-view routing without broad PR hydration. Allocate the first coherent slice and its fixture/provider/public-path selectors.

Governing design: [RFC-BPR-0026](../../rfcs/RFC-BPR-0026-personal-review-home-pr-url-navigation.md).
Closeout: [BPR-0599](../10-backlog/BPR-0599-close-rfc-0026.md).

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 4 of 4.
Planned cards: BPR-0598, BPR-0599. Allocate additional implementation cards only
when the later bounded-slice planning pass needs them, and update this list and
closeout dependencies before implementation. The RFC index remains the order
authority; this requested subset waives no intervening eligibility/closeout gate.

**Design accepted; registration only; implementation requires a later request.**
This unstarted backlog card records the 2026-09-04 registration, not an execution
grant or completed product work. The immutable base is the pre-registration
commit for this approved workstream; registration commits after it are planning
history, not evidence of interrupted product implementation.

## Acceptance

- Re-audit the live RFC dependencies, accepted scope, governing architecture, and
  current code/verification paths; record concrete ownership and migration facts.
- Map every numbered RFC acceptance criterion to a bounded implementation slice
  and a direct planned check, naming the smallest existing selectors or the
  focused new check required. Include one composed public path for UI behavior.
- Allocate only needed slice cards, link their dependencies and affected RFC,
  and update the closeout's planned cards/checklist without claiming tests ran.

## Non-goals

Product implementation, RFC closeout, live authentication or publication, broad
validation matrices, and redesigning the approved interaction model are not part
of this entry card. Later implementation authority does not waive the RFC's
existing dependency/eligibility rules.

## Verification record

Not run; entry work and product implementation are deferred. When authorized,
run the documentation gate on the planning slice, record exact focused selectors
for its implementation consumers, and commit with explicit paths. This card is
not satisfied merely by the initial RFC registration.

## Scoped reconnaissance, 2026-09-08

The original broad repository search still exists in
`src/adapters/github/src/github_paged_read.cpp`: the full PR read constructs
`search_pull_requests` with `SearchTerms = "is:open"` and reads SearchResults
before returning its snapshot. A saved-repository personal discovery scheduler
must be independent of this full-review path; filtering that snapshot is not
AC03/05 evidence. Existing core queue roles distinguish author/requested reviewer
but do not by themselves prove discovery before opening a PR.

BPR-0689 supplies the exact unknown-server/signed-out pending destination bridge
under RFC0018. It preserves unsupported sublocation context with an explanation;
exact accepted file/comment/commit mapping, profile/window Resume and personal
discovery remain RFC0026 work. Query parameters must not route credentials or
provider requests. This note is intake evidence, not a completed inventory or
product acceptance. No source or product check ran for this reconnaissance.
