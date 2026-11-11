---
schema: "implement-rfc.card.v1"
id: "DRAFT-0041"
title: "Create the draft PR from the pushed branch"
status: "Backlog"
rfcs: ["RFC-DRAFT-0014"]
depends_on: ["DRAFT-0037", "DRAFT-0040"]
metadata: {"criteria": ["AC01", "AC02", "AC07"], "resolved_choices": ["PUBLISH-01"], "milestone_plan": {"id": "v0-5-0-prerelease-targets-v1", "path": "docs/rfcs/assets/v0-5-0-prerelease-targets-v1/milestone-plan.json"}, "absorbed_from": "local PR drafts pack v1, DRAFT-0014"}
---

# Create the draft PR from the pushed branch

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0014](../RFC-DRAFT-0014-draft-publication-and-continuity.md), criteria `AC01`, `AC02`, `AC07`.

The explicit native publication capability and its exact preview: verify the remote head against the candidate, name the push to the hosted repository when it is missing or different, and create the draft PR through the admitted REST authority. BPR pushes nothing.

The RFC's section "Fitted to this pack's other decisions" is part of this card's scope.

## Intended verification

Create a draft from a pushed branch on the designated safe live target with the author's commits unchanged; show the named push for an unpushed branch, a different remote head and an isolated local clone. Do not infer host qualification from fixtures.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).

`PUBLISH-01` was answered on 2026-11-10: the author pushes and BPR creates the pull request from the verified remote head. See the [decision record](../local-drafts/DECISIONS.md#resolved-product-choices).
