---
schema: "implement-rfc.card.v1"
id: "DRAFT-0001"
title: "Decide and show whose move an authored pull request is"
status: "Backlog"
rfcs: ["RFC-DRAFT-0001"]
depends_on: []
---

# Decide and show whose move an authored pull request is

## Scope and acceptance

Implements
[RFC-DRAFT-0001](../RFC-DRAFT-0001-home-says-whose-move-my-pull-request-is.md).

Carry the review decision, merge state and reviewers' latest states from the
existing Home read into queue facts, add the bounded conversations-awaiting-
reply read for authored pull requests, decide the next move in the core by the
RFC's table, and present the reason word, sentence, tone, order and section
summary on Home.

Covers AC01 to AC05. AC02's placement is settled: authored rows stay in
My pull requests and Waiting on others, with no new section (owner,
2026-11-10). AC06 is the owner's
look on a frozen build. Load frontend-design before the Qt work.

This is part of `home-next-move` in the [plan](../milestone-plan.json).
