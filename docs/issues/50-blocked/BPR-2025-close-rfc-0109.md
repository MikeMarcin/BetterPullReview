---
schema: "implement-rfc.card.v1"
id: "BPR-2025"
title: "Verify and close RFC-BPR-0109"
status: "Blocked"
rfcs: ["RFC-BPR-0109"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0010","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0037-close-author-evidence-preparation.md","draft_title":"Close author evidence preparation"}
depends_on: ["BPR-2023", "BPR-2024"]
closeout_for: ["RFC-BPR-0109"]
blocked_by: "owner-deferred"
---

# BPR-2025: Verify and close RFC-BPR-0109

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0109](../../rfcs/RFC-BPR-0109-author-evidence.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`.

Acceptance assessment for the evidence RFC.

The RFC's section "Fitted to this pack's other decisions" is part of this card's scope.

## Intended verification

Judge all evidence criteria without reusing reviewer-verification state as proof of a check.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** An author records and revises claims, imports a targeted check result, follows its artifacts and resumes the evidence history without an agent, external service or existing PR. | Pending | Pending | Not implemented |
| **AC02:** Planned, reported, failed, interrupted and unbound results remain distinguishable; no path in this RFC labels a result observed, and an agent's claimed pass or zero exit cannot automatically verify a claim or approve a PR. | Pending | Pending | Not implemented |
| **AC03:** Dirty candidates and selected content bind evidence by actual material; relevant candidate changes require recheck while unchanged text-only edits preserve applicable code results and historical evidence remains inspectable. | Pending | Pending | Not implemented |
| **AC04:** Test, benchmark and formal/model evidence show their distinct scope; a model or bounded proof result cannot be displayed as proof of the whole implementation or PR. | Pending | Pending | Not implemented |
| **AC05:** Imported oversized/malformed records, missing artifacts, cancellation and stale completions produce recoverable outcomes without partial accepted records, secret-store reads, command execution or false current evidence. | Pending | Pending | Not implemented |
| **AC06:** The public preparation flow supports accepting or rejecting proposed claims and checks and retaining unresolved questions, with no agent run started by this RFC; evidence is selectable for later description and tour use without exposing unselected private history. | Pending | Pending | Not implemented |
| **AC07:** A test result produced by an Agent tab agent under its build allowance can enter the evidence record only as an imported, reported result bound to the material it tested; no path labels it observed. | Pending | Pending | Not implemented |

## Owner decision: deferred out of 0.5.0 (2026-11-12)

The owner, asked whether to cut Evidence from 0.5.0 after rejecting its look:
"Mockups; cut Evidence". [RFC-BPR-0109](../../rfcs/RFC-BPR-0109-author-evidence.md)
is Deferred and kept as a possible workflow extension; this card is Blocked on
`owner-deferred` and no 0.5.0 card depends on it. The evidence implementation
was removed from the product on the 0.5.0 lane; reactivating this card needs a
new owner decision and design, not a restore of that code.
