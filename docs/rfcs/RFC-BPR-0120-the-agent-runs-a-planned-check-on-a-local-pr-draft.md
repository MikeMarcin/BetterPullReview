---
id: "RFC-BPR-0120"
title: "The agent runs a planned check on a local PR draft"
status: "Proposed"
metadata: {"origin":"imported 2026-11-10 from the agent-assistance-v1 pack, where it was RFC-DRAFT-0005","inspected_revision":"90785ae34aeccf63724b9e5e7a70446f89341ec5","milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"baseline":"a9663a706fd72061f60b615bd8e749ee0ddb66b5"}
depends_on: ["RFC-BPR-0103", "RFC-BPR-0104", "RFC-BPR-0109", "RFC-BPR-0110", "RFC-BPR-0115"]
---

# The agent runs a planned check on a local PR draft

## Problem and grounded current behavior

A local draft's evidence can hold planned checks, and an agent can already
propose them: "a proposed claim or planned check is a record the author
accepts or rejects, whatever produced it: the author, an import, or later an
agent conversation"
([RFC-BPR-0109](RFC-BPR-0109-author-evidence.md)), and a whole-change
conversation proposes them under
[RFC-BPR-0110](RFC-BPR-0110-local-tours-and-description.md).

Nothing in the application runs one. RFC-BPR-0109 "adds no runner, and a
handoff to the author's full agent or terminal runs where BPR does not watch
it". The author runs the check elsewhere and types or imports the result.

[RFC-BPR-0115](RFC-BPR-0115-agent-changes-in-a-local-pr-draft-worktree.md)
now gives an agent a run in the author's own worktree, with builds and tests
where the author has turned them on. The owner asked that the agent run
proposed checks and record the results as reported evidence
([notes 2 and 3](assets/agent-assistance-v1/SOURCE_NOTES.md)).

## Proposed behavior and boundaries

A planned check in a local draft's Evidence that states a command or a
method offers **Run with agent**, an agent control.

- It is a change run under RFC-BPR-0115's **Edits this worktree** grant, and
  it needs **Build and run tests** on for that agent. Where either is
  missing the control is unavailable and says which.
- It starts only when the worktree holds what the current candidate
  describes, so the result has something to be bound to.
- The agent is asked to perform that one check and to report its outcome,
  the commands it ran and anything that limits the result. Its activity
  shows the commands as
  [RFC-BPR-0103](RFC-BPR-0103-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md)
  defines.
- The result is recorded as a **reported** result with the agent as its
  origin, in RFC-BPR-0109's terms. It is never an observed execution: that
  RFC's rule stands, that "reported success never becomes an observed
  execution solely because an agent emits structured JSON". Pass, fail,
  inconclusive, not run and interrupted are recorded as they happened, and a
  failed result is kept.
- The result is bound to the candidate only when the worktree's content
  equals the candidate both before and after the run. A run that changed
  files leaves the result unbound, says so, and lists the run's changes
  under RFC-BPR-0115.
- The result enters the evidence record only when the author accepts it,
  like any proposal. Nothing is published by running a check.
- It is briefed by a built-in prompt of its own,
  `local-draft.run-check` (a proposal; P02 in the
  [decision record](assets/agent-assistance-v1/DECISIONS.md#decisions)).

Running every planned check at once, running on a schedule or on refresh,
and observed executions that BPR supervises itself are not in this RFC.

## Acceptance criteria

- **AC01:** Run with agent is offered on a planned check with a command or method, and is unavailable with the specific reason when the worktree grant or Build and run tests is missing or the worktree does not hold the candidate.
- **AC02:** The result is recorded as reported, with the agent as its origin, the commands the run's activity shows and its stated limitations, and no agent-run result is ever recorded as observed.
- **AC03:** Failed, inconclusive, not-run and interrupted outcomes are recorded as such and remain visible in preparation.
- **AC04:** The result is bound to the candidate only when the worktree's content equals the candidate before and after the run; a run that changed files leaves it unbound with that reason and lists the changes.
- **AC05:** The result enters the evidence record only on the author's acceptance, and running a check publishes nothing.
- **AC06:** The operation is briefed by its own built-in prompt, whose edit reaches the next check run and no other prompt.
- **AC07:** On a frozen build with a fixture agent, the owner accepts the control, a passing result, a failing one and an unbound one.

## Proposed delivery

[Run with agent](../issues/10-backlog/BPR-2082-run-a-planned-check-with-the-agent.md) and
[closeout](../issues/10-backlog/BPR-2083-close-rfc-0120.md) form the fifth
[milestone](plans/agent-assistance-v1/milestone-plan.json).
