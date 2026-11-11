---
schema: "implement-rfc.card.v1"
id: "BPR-2070"
title: "History runs: merge target in, squash and rebase"
status: "Backlog"
rfcs: ["RFC-BPR-0115"]
metadata: {"criteria":["AC07","AC08","AC09","AC10","AC12"],"milestone_plan":{"id":"local-draft-authoring-v1","path":"docs/rfcs/plans/local-draft-authoring-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2062--BPR-2071--3110069c9cbb.json","imported_from":"docs/rfcs/assets/local-draft-authoring-v1/cards/DRAFT-0009-history-runs-merge-squash-rebase.md"}
depends_on: ["BPR-2068", "BPR-2066"]
---

# History runs: merge target in, squash and rebase

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0115](../../rfcs/RFC-BPR-0115-agent-changes-in-a-local-pr-draft-worktree.md), criteria `AC07`, `AC08`, `AC09`, `AC10`, `AC12`.

The three history runs, their preconditions and their further allowance; the record of the earlier head; the mechanical reports (complete or not, identical content after a squash, files changed on both sides after a merge or rebase, anything else that moved); and the stop before rewriting commits observed on the hosted branch.

## Intended verification

Fixture agents that perform each operation on real temporary repositories, including a merge left in progress, a squash that changes content and a rebase of pushed commits followed by a refused Push. The owner's look (AC12) is an attended review on a frozen build with the fixture agent.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/local-draft-authoring-v1/IMPLEMENTATION_PROMPT.md).
