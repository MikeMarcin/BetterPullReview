---
schema: "implement-rfc.card.v1"
id: "BPR-1398"
title: "Verify and close RFC-BPR-0070"
status: "In Progress"
rfcs: ["RFC-BPR-0070"]
depends_on: ["BPR-1396", "BPR-1397"]
closeout_for: ["RFC-BPR-0070"]
priority: "P1"
metadata: {"draft_pack": "merge-gates-and-review-waiting-v1", "rfc_criteria": ["AC01", "AC02", "AC03", "AC04", "AC05"], "mandate_revision": "merge-gates-r1", "rfc_base_sha": "9857d596769468c0585c475467175e3998a33530", "milestone_plan": {"id": "merge-gates-and-review-waiting-v1", "path": "docs/rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0070

## Scope and acceptance

Reconcile every criterion of the Home RFC and its explicit RFC0066 dependency. Finish the proposed Home/queue prose amendments and reconcile final pack coverage without rewriting historical evidence.

Authoritative acceptance: [RFC-BPR-0070](../../rfcs/RFC-BPR-0070-preserve-home-waiting.md#acceptance), AC01, AC02, AC03, AC04, AC05. Requirements stay in the RFC; this card claims no completed acceptance.

## Verification and handoff

Use one integration-focused pass across the three final outcomes plus the documentation gate, reusing unchanged milestone evidence. Preserve true platform and external limits. Live IDs, a real baseline, source revisions and acceptance records are created only during authorized execution.

Follow the [canonical milestone plan](../../rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json), [entrypoint](../../rfcs/assets/merge-gates-and-review-waiting-v1/IMPLEMENTATION_PROMPT.md) and [proposed amendments](../../rfcs/assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md). Keep this card in proposed Backlog until authorized import reconciles live dependencies and eligibility.

RFC base: `9857d596769468c0585c475467175e3998a33530`.

## Closeout checklist

| Criterion | Check | Result | Revision |
|---|---|---|---|
| **AC01:** An unopened authored PR discovered with pending reviews appears in Waiting on others. Opening, refreshing and marking its discussion read cannot replace its known requests with zero; Home and Overview agree. | Pending composed evidence | pending | — |
| **AC02:** An authored PR enrolled only by Follow or an observer rule gets the same waiting placement without saving its repository. An aggregate unmet-review requirement with no named request still produces a truthful waiting row; an unrelated account or team membership cannot supply that role. | Pending composed evidence | pending | — |
| **AC03:** The mixed Branch Freshness/code-owner example remains in Waiting on others with its team names and failure visible. Unread activity and its clearance follow the defined My pull requests precedence. Removing the last request does not hide an independently remaining review requirement or check problem behind Quiet. | Pending composed evidence | pending | — |
| **AC04:** Interleaved discovery/open/retained reads, head/account changes, incomplete or failed refresh, restart and old stored rows follow the availability rules. A complete newer removal or approval clears only the facts it actually settles; Home refresh stays bounded without full PR loads. | Pending composed evidence | pending | — |
| **AC05:** Composed Home preserves exact row selection, opening destination, accurate filtered/grouped counts, collapsed summaries and accessible waiting text. The Quiet-items filter, existing notification policy, unrelated enrollment and Recently viewed behavior follow the boundaries above. | Pending composed evidence | pending | — |

## Final pack obligation

The implementation mandate `merge-gates-r1` explicitly requires an independent critical review at the end of the complete pack. The coordinator dispatches that review separately; final pack acceptance must preserve and consume its actual findings and any verified corrections. The first milestone’s self-review and Auto-Verified status do not satisfy this obligation.
