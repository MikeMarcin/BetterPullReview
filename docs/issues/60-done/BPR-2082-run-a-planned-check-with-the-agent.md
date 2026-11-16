---
schema: "implement-rfc.card.v1"
id: "BPR-2082"
title: "Run a planned check with the agent"
status: "Done"
rfcs: ["RFC-BPR-0120", "RFC-BPR-0109"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0010-run-a-planned-check-with-the-agent.md","resolution":"rejected"}
depends_on: ["BPR-2024"]
---

# Run a planned check with the agent

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0120](../../rfcs/RFC-BPR-0120-the-agent-runs-a-planned-check-on-a-local-pr-draft.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`.

The control on a planned check; the change run that performs it; the reported result with its origin, commands and limitations; the binding rule; acceptance into the evidence record; and its own built-in prompt, `local-draft.run-check`. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

A fixture agent that passes, fails, is interrupted and changes a file, against real temporary worktrees; the recorded class is reported in every case. The owner's look (AC07) is an attended review on a frozen build with the fixture agent.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).

Note, 2026-11-12: its prerequisite RFC-BPR-0109 is Deferred out of 0.5.0 by the owner ("Mockups; cut Evidence") and BPR-2024 is Blocked on `owner-deferred`, so this card waits on that RFC's reactivation. (Superseded below.)

## Closed as rejected: moot (2026-11-15)

The owner rejected RFC-BPR-0109 on 2026-11-15: "it shouldnt be deferred it
should be rejected or something stronger so it doesnt come back". Earlier, on
its look: "I have no idea what I'm looking at here. This doesn't seem
useful...", and later "that seemed like useless". This card's whole scope is
running a planned check from that RFC's Evidence and accepting the result into
its evidence record, and it depended on BPR-2024, which is closed as rejected.
With no evidence record there is nothing for it to do, so it is moot. It is
closed in Done only because Done is the board's one terminal lane (its other
dependency, BPR-2068, is dropped: a closed card waits on nothing):
`metadata.resolution` is `"rejected"`, nothing was delivered, and it is not to
be reopened. The owner then directed, the same day, "Rewrite without evidence":
[RFC-BPR-0120](../../rfcs/RFC-BPR-0120-the-agent-runs-a-planned-check-on-a-local-pr-draft.md)
was rewritten and its new implementation is
[BPR-2189](../40-parked/BPR-2189-plan-and-run-a-check-on-a-local-draft.md). This
card stays closed as rejected; nothing depends on it.
