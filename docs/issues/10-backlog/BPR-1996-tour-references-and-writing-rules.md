---
schema: "implement-rfc.card.v1"
id: "BPR-1996"
title: "Link and publish code references in tour text, and add the citation and moved-code rules"
status: "Backlog"
rfcs: ["RFC-BPR-0100"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0008-tour-references-and-writing-rules.md"}
depends_on: ["BPR-1998"]
---

# Link and publish code references in tour text, and add the citation and moved-code rules

## Scope and acceptance

Implements
[RFC-BPR-0100](../../rfcs/RFC-BPR-0100-tour-conversation-that-changes-the-tour-in-one-turn.md).

Apply the shared reference rules to stop text, the Problem and the Solution;
show a reference alone on its line as a snippet; publish references as commit-
pinned permalinks in the form established on MikeMarcin/BPR-Test; report
unresolvable references before publication; add the citation rule and the
moved-code rule to the drafting prompt, the conversation contract and the
stop-comment hook and advance the guide and hook versions.

Covers AC06 to AC08. AC08 is the owner's judgement of a drafted tour on a
fixture pull request that moves an unchanged path behind a flag.

This is part of `tour-conversation` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
