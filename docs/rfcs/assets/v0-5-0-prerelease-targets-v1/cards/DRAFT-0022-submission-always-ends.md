---
schema: "implement-rfc.card.v1"
id: "DRAFT-0022"
title: "Find the stalled submission and make every submission end in a stated condition"
status: "Backlog"
rfcs: ["RFC-DRAFT-0008"]
depends_on: []
---

# Find the stalled submission and make every submission end in a stated condition

## Scope and acceptance

Implements
[RFC-DRAFT-0008](../RFC-DRAFT-0008-a-review-opens-fresh-and-a-submission-always-ends.md).

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

This is part of `dependable-session` in the [plan](../milestone-plan.json).
