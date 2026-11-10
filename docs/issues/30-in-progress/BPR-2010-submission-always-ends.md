---
schema: "implement-rfc.card.v1"
id: "BPR-2010"
title: "Find the stalled submission and make every submission end in a stated condition"
status: "In Progress"
rfcs: ["RFC-BPR-0105"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0022-submission-always-ends.md"}
depends_on: []
---

# Find the stalled submission and make every submission end in a stated condition

## Scope and acceptance

Implements
[RFC-BPR-0105](../../rfcs/RFC-BPR-0105-a-review-opens-fresh-and-a-submission-always-ends.md).

Start with the diagnosis the RFC requires: reproduce the stall with a long
multi-pull-request course or record the cause found by other means, and amend
the RFC's cause section if it differs. Then show Waiting, Sending, Published
or Not published for every submission, bound the wait to start at 20 seconds,
end the visible submission when a completion no longer matches, keep another
pull request's effects from blocking this one's submission, and log the
request, waits, drops and ending.

Covers AC01 to AC05 and the owner's course in AC08. Give this path the
repository's highest scrutiny: it publishes under the reviewer's account. Use
fixtures and MikeMarcin/BPR-Test only.

This is part of `dependable-session` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
