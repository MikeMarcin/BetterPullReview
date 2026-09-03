---
id: BPR-0538
title: Clear a workshop press when its turn ends without a revision
status: Backlog
rfc: RFC-BPR-0012
depends_on: [BPR-0533]
---

# Clear a workshop press when its turn ends without a revision

## Why

[[BPR-0533]]'s second half made a control press start one agent turn and
close the six workshop controls, with a tooltip, until that turn's
proposal arrives. The press is cleared when the dispatch is refused and
when the paired outcome is converted; it is not cleared when the turn
starts and then finishes without producing a revision. Until the reviewer
edits the draft, the six stay closed with a tooltip that is no longer
true. Clearing on finish needs a third sink out of
`composition_agent_invocation_projection`, which the slice did not add.

## Required final state

- A workshop turn that finishes without a revision — or is refused after
  it started, or whose session goes stale — clears the pending press and
  reopens the six controls, through the composition's existing invocation
  projection rather than a poll.
- The refusal presentation says why the turn produced nothing, in the
  same place the proposal would have appeared.
- `application_agent_finding_workshop_press_contract` gains the finished-
  without-revision course, and the editor test proves the six reopen; a
  second, unrelated session's outcome is proven not to consume a bound
  press.

## Non-goals

- Retrying the turn, or changing how a proposal is accepted or rejected.
