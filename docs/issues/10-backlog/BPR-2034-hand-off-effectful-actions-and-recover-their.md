---
schema: "implement-rfc.card.v1"
id: "BPR-2034"
title: "Hand off effectful actions and recover their outcomes"
status: "Backlog"
rfcs: ["RFC-BPR-0112"]
metadata: {"criteria":["AC05","AC06","AC08"],"resolved_choices":["ACTIONS-01"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0019","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0046-hand-off-effectful-actions-and-recover-their.md"}
depends_on: ["BPR-2031", "BPR-2033"]
---

# Hand off effectful actions and recover their outcomes

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0112](../../rfcs/RFC-BPR-0112-optional-workflow-extensions.md), criteria `AC05`, `AC06`, `AC08`.

Interactive handoff of effectful actions to the configured harness, current-plan authority and external result reconciliation, plus end-to-end built-in independence after native publication is delivered. No BPR-supervised effectful action.

## Intended verification

Demonstrate one publisher, truthful runtime capability, uncertain outcome recovery and independent remaining follow-up with a fixture companion. Also complete worktree-to-GitHub-draft publication with extensions absent and unselected; separately configured drafts retain their optional integration.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).

`ACTIONS-01` was answered on 2026-11-10: effectful actions use an interactive handoff. See the [decision record](../../rfcs/assets/v0-5-0-prerelease-targets-v1/local-drafts/DECISIONS.md#resolved-product-choices).
