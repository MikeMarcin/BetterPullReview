---
schema: "implement-rfc.card.v1"
id: "BPR-1989"
title: "Decide and show whose move an authored pull request is"
status: "In Progress"
rfcs: ["RFC-BPR-0098"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0001-home-next-move.md"}
depends_on: []
---

# Decide and show whose move an authored pull request is

## Scope and acceptance

Implements
[RFC-BPR-0098](../../rfcs/RFC-BPR-0098-home-says-whose-move-my-pull-request-is.md).

Carry the review decision, merge state and reviewers' latest states from the
existing Home read into queue facts, add the bounded conversations-awaiting-
reply read for authored pull requests, decide the next move in the core by the
RFC's table, and present the reason word, sentence, tone, order and section
summary on Home.

Covers AC01 to AC05. AC02's placement is settled: authored rows stay in
My pull requests and Waiting on others, with no new section (owner,
2026-11-10). AC06 is the owner's
look on a frozen build. Load frontend-design before the Qt work.

This is part of `home-next-move` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
