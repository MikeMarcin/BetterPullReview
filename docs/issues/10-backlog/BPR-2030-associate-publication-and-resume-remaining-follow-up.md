---
schema: "implement-rfc.card.v1"
id: "BPR-2030"
title: "Associate publication and resume remaining follow-up"
status: "Backlog"
rfcs: ["RFC-BPR-0111"]
metadata: {"criteria":["AC03","AC04","AC05","AC06"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0015","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0042-associate-publication-and-resume-remaining-follow-up.md"}
depends_on: ["BPR-2029"]
---

# Associate publication and resume remaining follow-up

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0111](../../rfcs/RFC-BPR-0111-draft-publication-and-continuity.md), criteria `AC03`, `AC04`, `AC05`, `AC06`.

Publication journal integration, actual candidate/PR association, evidence/tour reconciliation and incomplete follow-up.

## Intended verification

Exercise lost answers, created-PR/failed-follow-up and remote drift; preserve private preparation and avoid duplicate creation.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

## Implementation record (not claimed)

Engineered on the lane while its dependencies wait (the owner rejected the
RFC-BPR-0109 and 0110 looks on 2026-11-12); this card was not readied or
claimed. Commits `387f7298d` (core), `4675974cf` and `62f740fc6` (gh pull
request catalog), `6bc5217f5` and `433068905` (application), `36dcd7e21` (Qt
area, composition), `75c035a49` (real-worktree course). Evidence, including
the live BPR-Test drafts #178 and #179: [ledger](../../verification/RFC-BPR-0111.md).

Owner amendment, 2026-11-12 ("Mockups; cut Evidence"): RFC-BPR-0109 is Deferred; reconciliation covers the tour only (RFC-BPR-0111 AC03 amended).
