---
schema: "implement-rfc.card.v1"
id: "BPR-1321"
title: "Repair evidence-backed GHES actor observation admission"
status: "Blocked"
rfcs: ["RFC-BPR-0065"]
depends_on: ["BPR-1319"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r4"}
blocked_by: "ghes-actor-provenance"
---

# Repair evidence-backed GHES actor observation admission

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Synthetic attributable actor conflict is repaired at its owning boundary only under established representation/provenance facts; true conflicting observations still reject.
- C2: Composed public PR load, restart/retry and stale completion controls pass with stable actor IDs, exact authority and other relationship validation intact.

## External evidence boundary

Blocked on `ghes-actor-provenance`. BPR-1319 reproduces viewer/review origin
conflicts separately for case-only and different login observations, while
all original relationship rejection remains intact. The private original
exports establish the failure category/counts but omit actor values and their
originating collections. They do not establish which representation is
legitimate. New closed diagnostics identify only origin classes and a case-only
fact, with no login, actor ID, private target or raw response exported.

No normalization or alias rule is inferred from the Git support-policy change.
C1 requires attributable legitimate representation/provenance facts before an
admission repair; C2 requires that repair through the composed PR read path.
Both remain pending. The coordinator owns collection of affected configured
paths/source-build facts and a new read-only failure observation. Source
`e61614205` supplies the privacy-safe diagnostic foundation; combined
`980c08873` repairs the independent Git capability gate and leaves true actor
conflicts rejecting. No private log/content is staged or committed.
