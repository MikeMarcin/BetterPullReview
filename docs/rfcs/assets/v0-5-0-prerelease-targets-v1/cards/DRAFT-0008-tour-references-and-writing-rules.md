---
schema: "implement-rfc.card.v1"
id: "DRAFT-0008"
title: "Link and publish code references in tour text, and add the citation and moved-code rules"
status: "Backlog"
rfcs: ["RFC-DRAFT-0003"]
depends_on: ["DRAFT-0010"]
---

# Link and publish code references in tour text, and add the citation and moved-code rules

## Scope and acceptance

Implements
[RFC-DRAFT-0003](../RFC-DRAFT-0003-tour-conversation-that-changes-the-tour-in-one-turn.md).

Apply the shared reference rules to stop text, the Problem and the Solution;
show a reference alone on its line as a snippet; publish references as commit-
pinned permalinks in the form established on MikeMarcin/BPR-Test; report
unresolvable references before publication; add the citation rule and the
moved-code rule to the drafting prompt, the conversation contract and the
stop-comment hook and advance the guide and hook versions.

Covers AC06 to AC08. AC08 is the owner's judgement of a drafted tour on a
fixture pull request that moves an unchanged path behind a flag.

This is part of `tour-conversation` in the [plan](../milestone-plan.json).
