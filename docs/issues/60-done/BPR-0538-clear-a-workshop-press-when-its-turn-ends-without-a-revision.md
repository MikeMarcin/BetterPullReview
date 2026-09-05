---
schema: "implement-rfc.card.v1"
id: "BPR-0538"
title: "Clear a workshop press when its turn ends without a revision"
status: "Done"
rfcs: ["RFC-BPR-0012"]
depends_on: ["BPR-0533"]
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

## Landed — 2026-09-04

At `e5e78c3f`, reproduced on the merged tree by the integrator.

**The third sink.** `composition_agent_invocation_projection` gained a run
sink, invoked from `finish` after the panel's own, whose lambda calls the new
`settle_comment_workshop(session, outcome)`. That method returns for a press
that is unbound or bound to another session — the unrelated-turn guard — then
clears `Running` so the six controls reopen, and drops the record with a
stated reason when the turn produced nothing: no delivery, or any outcome
other than success, which covers a run refused after it started and a session
gone stale. The press record gained `Running` and `Delivered`; `Delivered` is
set by the existing findings sink only when the delivery's session is the
bound one and carries findings, so a turn whose finding is still waiting for
the reviewer's conversion keeps its label pairing and [[BPR-0533]]'s course is
unchanged.

**The presentation needed Qt.** `WorkshopAsked` is private editor state, so
nothing outside the widget could reopen the six. A new
`comment_workshop_settlement` and `review_draft_editor::present` for it
ignores a settlement naming another draft, clears the flag, and writes
`No revision: <reason>` into the operation label — the same line where
`Proposed by:` appears beside the before and after panes — only when a reason
is given. The proposal pane stays empty, and no member was added, so the
editor's members aggregate and its `sizeof` pin are untouched.

| Claim | Command | Result | Commit |
|---|---|---|---|
| A turn that ends without a revision reopens the six | `qt_review_draft_editor` — new case | passes; pre-fix `FAILED: and all six reopen when it ends, without waiting for a proposal that is not coming` | this commit |
| The composed course presses again after a turn that produced nothing | `application_agent_finding_workshop_press_contract`, stages two to four | passes; pre-fix `agent-finding-composition-failure:workshop-second-clarify-control` — the ended turn left the six shut so stage three could not press | this commit |
| An unrelated session's outcome does not consume a bound press | stage four: a launch-panel turn delivers and is converted while stage three's press is bound | the press survives (`unrelated-pending:clarify:bound`) and the proposal names no operation | this commit |
| Core, host, Qt, and composition rows | `ctest --preset dev -j 2` over the ten-row agent set | 10/10 | this commit |
| The composed round trip and its two pins | that contract, run alone | passed, 35.6 s | this commit |
| Boundaries, tidy, format | `check_boundaries`; `tidy.py`; `clang-format --Werror` | exit 0; exit 0 after one local rename; exit 0 | this commit |

**Left open, recorded rather than claimed.** The negative half of the third
row already held at the previous head — [[BPR-0533]]'s session check is what
stops a conversion consuming another session's press — so it is proven here
end to end rather than red before the fix, and that is stated plainly instead
of being staged into a failure. The finished-without-revision course is
stages two to four of the existing row rather than a new registration,
because a new ctest name would need the network-isolation manifest script the
slice was not allowed to touch; a distinct row is available whenever that is
wanted. The press course now drives four agent turns, 17 s against a 120 s
bound. A press whose turn delivered a finding the reviewer never converts
keeps its label pairing while the six reopen, which is deliberate and has no
test of its own.

