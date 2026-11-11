---
schema: "implement-rfc.card.v1"
id: "BPR-2083"
title: "Verify and close RFC-BPR-0120"
status: "Backlog"
rfcs: ["RFC-BPR-0120"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0011-close-agent-run-checks.md","draft_title":"Close agent-run checks"}
depends_on: ["BPR-2082"]
closeout_for: ["RFC-BPR-0120"]
---

# BPR-2083: Verify and close RFC-BPR-0120

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0120](../../rfcs/RFC-BPR-0120-the-agent-runs-a-planned-check-on-a-local-pr-draft.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`.

Acceptance assessment for running a planned check with the agent.

## Intended verification

Judge every criterion on the composed application with an installed agent as well as the fixture agent.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `a9663a706fd72061f60b615bd8e749ee0ddb66b5`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Run with agent is offered on a planned check with a command or method, and is unavailable with the specific reason when the worktree grant or Build and run tests is missing or the worktree does not hold the candidate. | Pending | Pending | Not implemented |
| **AC02:** The result is recorded as reported, with the agent as its origin, the commands the run's activity shows and its stated limitations, and no agent-run result is ever recorded as observed. | Pending | Pending | Not implemented |
| **AC03:** Failed, inconclusive, not-run and interrupted outcomes are recorded as such and remain visible in preparation. | Pending | Pending | Not implemented |
| **AC04:** The result is bound to the candidate only when the worktree's content equals the candidate before and after the run; a run that changed files leaves it unbound with that reason and lists the changes. | Pending | Pending | Not implemented |
| **AC05:** The result enters the evidence record only on the author's acceptance, and running a check publishes nothing. | Pending | Pending | Not implemented |
| **AC06:** The operation is briefed by its own built-in prompt, whose edit reaches the next check run and no other prompt. | Pending | Pending | Not implemented |
| **AC07:** On a frozen build with a fixture agent, the owner accepts the control, a passing result, a failing one and an unbound one. | Pending | Pending | Not implemented |
