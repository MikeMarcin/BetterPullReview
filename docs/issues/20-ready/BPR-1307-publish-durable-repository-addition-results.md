---
schema: "implement-rfc.card.v1"
id: "BPR-1307"
title: "Publish exact durable repository addition results"
status: "Ready"
rfcs: ["RFC-BPR-0062"]
depends_on: ["BPR-1306"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
---

# Publish exact durable repository addition results

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: A signed-in session with no open PR saves through the existing queue store; saving, duplicate, local refusal and retry have exact typed results.
- C2: Successful records survive SQLite reopen and reach sibling profile windows; pending and late completions cannot confirm a save for a replacement authority.
