---
id: "RFC-BPR-0103"
title: "Agent work is visible, and a turn ends when the work does"
status: "Auto-Verified"
metadata: {"origin":"owner notes, 2026-11-10, note 18","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"mandate_revision":"v0-5-0-prerelease-targets-r1","baseline":"dc5453afa3ba0e1620fe30bf97b17211e09ba4bd"}
---

# Agent work is visible, and a turn ends when the work does

## Problem and grounded current behavior

A review of a large pull request answered "I've started three parallel
reviewers". Nothing followed for fifteen minutes, the owner's spend tracker
did not move, and the Agent tab gave no sign of whether anything was running
([note 18](assets/v0-5-0-prerelease-targets-v1/SOURCE_NOTES.md)). In the inspected code:

- The Claude decoder (`decode_claude_frame`,
  [agent_launch.cpp](../../src/application/src/agent_launch.cpp))
  processes `system/init`, `assistant`, `user` and `result` frames. Every
  other frame type is read past, including the task and tool-progress
  notifications. Thinking is dropped. Tool use is tracked only for read-only
  tour runs; the Agent tab receives none.
- A turn is complete at the first `result` frame. On a native connection the
  loop then returns success
  ([agent_protocol_loop.cpp](../../src/adapters/agent/src/agent_protocol_loop.cpp)).
  A later `assistant`, `user` or `result` frame is a malformed frame, and
  the idle drain ends the connection when any event arrives after the turn.
  RFC-BPR-0096 states the rule: "A second result or late frame cannot be
  accepted as the next turn."
- The ordinary Agent tab run does not disallow the sub-agent tool; only the
  tour's read-only mode does
  ([agent_launch.hpp](../../src/application/include/bpr/application/agent_launch.hpp)).
- The tab's status is one line, "*Agent* is thinking", "writing" or "making
  its workspace", with a clock that ticks whether or not anything arrives
  ([agent_run_status.cpp](../../src/adapters/qt/src/agent_run_status.cpp)).
- A run has no time limit unless the reviewer sets one (owner decision,
  2026-10-23).

Together these give a likely account of the report, read from the code and
not reproduced: the agent started background helpers and ended its message;
BPR took the `result` as the end of the turn; the helpers' later frames were
discarded or ended the connection; nothing was shown. How the installed
Claude and Codex actually report background work on these transports is not
established.

## Proposed behavior and boundaries

The frames are in the
[design round](../design/v0-5-0-targets-v1/index.html#agent). This RFC
applies to the Agent tab and the tour side chat.

### What a running turn shows

While a turn runs, its place in the transcript shows:

- **What the agent is doing**, from the runtime's own events: starting its
  workspace, thinking, reading a named file, running a named command,
  writing its answer, or waiting on helpers.
- **How long**, and **when it last showed a sign of life**: the time since
  the last frame of any kind from the runtime ("last activity 4 s ago").
- **Its helpers**: one row for each sub-agent or background task the runtime
  reports, with the helper's own description, its state (running, finished,
  failed), what it last did, and its elapsed time.
- **Stop**, which ends the turn and keeps what arrived.

Thinking is shown as a state. Its text is not shown; that is deferred.

**Amendment, 2026-11-11 ([[BPR-2051]], owner decision "A + footer 1").** A
running summary is also in the pull request window's footer, whatever drawer
is open: where the run started (Agent tab, Tour chat, Enhance, Draft the
tour, Tour tab), what it is doing ("waiting on 2 helpers"), how long, and
Stop. Several runs at once read as a count ("2 running: Agent tab,
Enhance") that opens the list, each with its own Stop. After 120 seconds
without a frame the footer's run turns the quiet tone and says "no activity
for 2 min"; nothing else in the footer moves. The drawers keep the detail
this section describes (the helpers' rows, the folded line); the footer is
a summary of it, not a second copy.

After the turn, the block folds to one line under the answer ("Worked for 7
min: 3 helpers, 41 files read, 12 commands") that opens to the same rows.

### When a turn ends

A turn ends when the runtime reports it finished **and** no helper it
reported is still running. Text the agent writes before its helpers finish
is shown as it arrives and is part of the same turn. The helpers' results
and the agent's closing message arrive in that turn, and its duration and
cost cover all of it.

This amends RFC-BPR-0096's late-frame rule in one respect: frames that
continue a turn whose helpers are outstanding are that turn's frames. A
frame that belongs to no open turn is still refused.

Where a runtime does not report background work in a way that lets BPR know
whether helpers are outstanding, BPR runs that agent **foreground only**:
the launch disallows or instructs against detached helpers, so a result
means the work is done. Which installed agents need this is established
first, on their real transports, and recorded.

### Quiet is said out loud

When the runtime has sent nothing for 120 seconds, the block changes to a
notice in the attention role: "No activity for 2 min", with what the agent
was last doing and the choices **Stop and keep what arrived** and **Keep
waiting**. The notice clears when a frame arrives and returns after another
120 seconds of silence. It never stops the run by itself; the no-limit
default stands.

### A turn cannot end silently

Every turn ends in exactly one of: an answer; Stopped by the reviewer; or an
error that says what happened. A connection that closes, a process that
exits, or a turn that finishes with helpers that never reported back ends
as an error, for example "Claude stopped before its 2 remaining helpers
finished. What arrived is kept." It is never left as a clock, and never
shown as an answer that promises more.

### Diagnosis

The run's diagnosis line gains the helper counts (started, finished,
failed, abandoned), the longest silence, and the frame types that were read
past, so a report like this one can be explained from a diagnostic export.

### Boundaries

- No new permission is granted to any agent, and no tool is enabled that
  was not available before.
- The one-shot tool-free hooks are unchanged.
- The foreground-only fallback is a property of an agent and its transport,
  shown in that agent's run details. It is not a reviewer setting.

## Implementation notes (2026-11-10)

Established, not assumed ([verification](../verification/RFC-BPR-0103.md)):
Claude Code 2.1.319 reports helpers on stream JSON and answers each helper's
report in a further part of the same turn, so it runs with helpers visible.
Codex 0.167.0 reports sub-agents on the App Server but never answers them by
itself, and reports nothing on `exec --json`; no flag of that version removes
its sub-agent tools. Codex runs are therefore asked to keep helpers in the
foreground (an instruction, not an enforcement), and on the App Server the
turn also stays open until every reported helper has completed.

**Stop** is the Agent tab's and the side chat's own Stop (the Send control
while a turn runs) and the quiet notice's **Stop and keep what arrived**; the
block adds no third control. A turn's folded line and how it ended are kept
when this window draws the thread again from its history; they are not saved
with the history, so a restarted application shows the history's answer
without them.

### Amendment: a sign-in failure ends the turn at once (owner, 2026-11-12, [[BPR-2142]])

From the owner's corporate wrapper tests (2026-11-11; the correction of the
same day supersedes the first report): after about 25 headless runs the
wrapper failed every run with about ten `api_retry` events carrying
`authentication_failed` and then `401 {"error":"missing <gateway> token"}`,
after several minutes, sometimes as a stream-JSON `result` and sometimes as
plain text on standard output. BPR reported it as an answer in a form it
cannot read. Owner decision: report it as a sign-in problem, and stop at the
first `authentication_failed` retry instead of waiting minutes.

- A Claude `system/api_retry` frame whose error is `authentication_failed`
  ends the run at once as a sign-in failure; the process is stopped.
- A `result` that is an error naming a 401, `authentication_failed`, "Not
  logged in" or "Invalid API key", and a run that wrote no frame but whose
  plain-text output names a 401 or an authentication failure, end as a
  sign-in failure too, never as "answered in a form this app can't read".
- The words say the agent is not signed in, or its wrapper could not get a
  token, and what to do: sign in again in a terminal, or try again later.

### Amendment: accepted steering is part of the turn (RFC-BPR-0126, 2026-11-16)

[RFC-BPR-0126](RFC-BPR-0126-queue-follow-ups-and-steer-native-agent-chats.md)
adds accepted steering to the turn boundary above. A turn into which the
reviewer steered a correction ends when the runtime reports it finished,
no reported helper is running **and** every correction it accepted has a
terminal disposition: consumed (Claude names it in `user_message_uuids`;
Codex App Server answers `turn/steer` for the running turn, then
`turn/completed`), refused before it was taken, or Delivery unknown. A
correction Claude takes into a successor native turn keeps that turn in the
same BPR run, under its lease, limits and cost; the run waits for that
successor at most 30 seconds, and an unanswered `turn/steer` at most 30
seconds, after which the correction is Delivery unknown and the connection is
retired ([[BPR-2152]], [[BPR-2153]]). A queued message waiting behind the turn
is released only after this end and the host's retirement, never on a raw
`result` or a timer. A steered message is shown in the conversation as the
author's at once, after the step it followed ([[BPR-2154]]).

## Acceptance criteria

- **AC01:** For each installed agent and transport BPR supports, a recorded
  course states how background work is reported and whether the agent is run
  with helpers visible or foreground only.
- **AC02:** With a scripted runtime that starts three helpers, writes a
  message, and reports `result` before they finish, the turn stays running,
  shows three helper rows with their states, accepts the later frames, and
  ends once after the closing message with one duration and one cost.
- **AC03:** With the same script and one helper that never finishes before
  the connection closes, the turn ends as an error that names the unfinished
  helper and keeps the text that arrived.
- **AC04:** A frame that arrives when no turn is open is refused as before.
- **AC05:** After 120 seconds without a frame the notice appears with both
  choices; a new frame clears it; Keep waiting leaves the run untouched.
- **AC06:** During a turn the block names the file being read or the command
  being run as the runtime reports them, and "last activity" resets on every
  frame.
- **AC07:** With an installed agent on MikeMarcin/BPR-Test, a request that
  makes the agent start helpers either shows them and completes in one turn,
  or runs foreground only and completes in one turn.
- **AC08:** The tour side chat shows the same block and the same endings.
- **AC09:** The owner accepts the running, quiet and ended states on a
  frozen build.
- **AC10:** (amended 2026-11-12) A scripted Claude run that reports one
  `api_retry` with `authentication_failed` ends at that frame as a sign-in
  failure; a run whose only output is a plain-text 401 and one whose
  `result` is a 401 error end the same way.
