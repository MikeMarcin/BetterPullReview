---
schema: "implement-rfc.card.v1"
id: "BPR-2085"
title: "Verify and close RFC-BPR-0121"
status: "Backlog"
rfcs: ["RFC-BPR-0121"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0013-close-agent-insights.md","draft_title":"Close the Agents section in Insights"}
depends_on: ["BPR-2084"]
closeout_for: ["RFC-BPR-0121"]
---

# BPR-2085: Verify and close RFC-BPR-0121

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0121](../../rfcs/RFC-BPR-0121-insights-shows-what-the-agents-did-and-what-was-kept.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`.

Acceptance assessment for the Agents section.

## Intended verification

Judge every criterion on the composed application, and compare one real session's Agent tab cost with the section's sum.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `a9663a706fd72061f60b615bd8e749ee0ddb66b5`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** For a fixture session, the Agents section's runs per feature and their endings equal the runs that session started and how each ended. | Pending | Pending | Not implemented |
| **AC02:** Spend per feature equals the sum of the costs the agent reported for its runs, and a run with no reported cost shows as Not reported and adds nothing to the sum. | Pending | Pending | Not implemented |
| **AC03:** Offered, kept, dismissed and neither are counted for finding cards, Enhance and tour proposals, each once and never decreasing, and keeping then removing a kept item does not subtract. | Pending | Pending | Not implemented |
| **AC04:** The counts start with the first run of a build that has the section, follow the review stats' purge and reset rules, and the anonymized export holds counts and sums only. | Pending | Pending | Not implemented |
| **AC05:** An agent feature with no Insights row fails a test. | Pending | Pending | Not implemented |
| **AC06:** On a frozen build with seeded counts, the owner accepts the Agents section. | Pending | Pending | Not implemented |
