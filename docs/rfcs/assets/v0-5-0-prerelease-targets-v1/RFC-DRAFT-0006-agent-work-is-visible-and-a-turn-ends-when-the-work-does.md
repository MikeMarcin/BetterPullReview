---
id: "RFC-DRAFT-0006"
title: "Agent work is visible, and a turn ends when the work does"
status: "Proposed"
metadata: {"origin":"owner notes, 2026-11-10, note 18","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"authoring":"draft only; no implementation authorized"}
---

# Agent work is visible, and a turn ends when the work does

## Problem and grounded current behavior

A review of a large pull request answered "I've started three parallel
reviewers". Nothing followed for fifteen minutes, the owner's spend tracker
did not move, and the Agent tab gave no sign of whether anything was running
([note 18](SOURCE_NOTES.md)). In the inspected code:

- The Claude decoder (`decode_claude_frame`,
  [agent_launch.cpp](../../../../src/application/src/agent_launch.cpp))
  processes `system/init`, `assistant`, `user` and `result` frames. Every
  other frame type is read past, including the task and tool-progress
  notifications. Thinking is dropped. Tool use is tracked only for read-only
  tour runs; the Agent tab receives none.
- A turn is complete at the first `result` frame. On a native connection the
  loop then returns success
  ([agent_protocol_loop.cpp](../../../../src/adapters/agent/src/agent_protocol_loop.cpp)).
  A later `assistant`, `user` or `result` frame is a malformed frame, and
  the idle drain ends the connection when any event arrives after the turn.
  RFC-BPR-0096 states the rule: "A second result or late frame cannot be
  accepted as the next turn."
- The ordinary Agent tab run does not disallow the sub-agent tool; only the
  tour's read-only mode does
  ([agent_launch.hpp](../../../../src/application/include/bpr/application/agent_launch.hpp)).
- The tab's status is one line, "*Agent* is thinking", "writing" or "making
  its workspace", with a clock that ticks whether or not anything arrives
  ([agent_run_status.cpp](../../../../src/adapters/qt/src/agent_run_status.cpp)).
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
[design round](../../../design/v0-5-0-targets-v1/index.html#agent). This RFC
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

## Acceptance

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
