---
schema: "implement-rfc.card.v1"
id: "BPR-2038"
title: "Fit the Agent tab's turn and session footers on one line each"
status: "In Progress"
rfcs: ["RFC-BPR-0101"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2038--BPR-2039--0af453354534.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"origin":"owner rejection of BPR-1999-agent-footers, attended session fd2f7fdb4d5b89ca67e9793084a68d04, decision 3dd7f36f (event 54)"}
depends_on: ["BPR-1999"]
---

# Fit the Agent tab's turn and session footers on one line each

## Scope and acceptance

Implements the owner's correction to [RFC-BPR-0101](../../rfcs/RFC-BPR-0101-agent-chat-conventions.md)
AC08. The owner rejected the footers of [[BPR-1999]] on the frozen build
(session `fd2f7fdb4d5b89ca67e9793084a68d04`, decision event 54):

> It's taking up too much space. We might need things to go in tooltips or
> shorten. Like just "Claude answered in". Maybe "under" becomes "<". My goal
> is one line for both turn and session status lines at this default width.
> And this agent can read whole PR thing is way too long. I think that
> certainly needs to be a tooltip message instead.

At the default window width, a turn's footer and the session line each take
one line. The turn footer names the agent as "Claude", with the model and
other detail in its tooltip; a nonzero cost under half a cent reads
`< $0.01`; the note that the agent can read the whole pull request moves to a
tooltip. Amend the RFC's format rule and AC04 for `< $0.01`.

Covers the footer half of AC08; the accent half was accepted with
[[BPR-2000]]. Load frontend-design before the Qt work.

This is part of `agent-tab-asks` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
