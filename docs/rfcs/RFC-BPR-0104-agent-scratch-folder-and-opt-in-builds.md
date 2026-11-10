---
id: "RFC-BPR-0104"
title: "A scratch folder for agent scripts, and opt-in builds and tests"
status: "Proposed"
metadata: {"origin":"owner notes, 2026-11-10, note 15","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"mandate_revision":"v0-5-0-prerelease-targets-r1","baseline":"dc5453afa3ba0e1620fe30bf97b17211e09ba4bd"}
---

# A scratch folder for agent scripts, and opt-in builds and tests

## Problem and grounded current behavior

An agent said "I can't write files in this session, so a script-based parity
check is out" and then did the check a slower way. The owner asks whether an
agent can write disposable scripts without touching tracked files, and would
like to be able to let it build and run tests
([note 15](assets/v0-5-0-prerelease-targets-v1/SOURCE_NOTES.md)). In the inspected code:

- An ordinary Agent tab run launches Claude with `-p`, stream-JSON input and
  output, and the model and effort; a native session adds `--resume`
  ([agent_launch.cpp](../../src/application/src/agent_launch.cpp)).
  It passes no permission mode, no allowed or disallowed tools and no added
  directory. What the agent may do is whatever the reviewer's own agent
  configuration allows without a prompt, since a headless run cannot ask.
  That is the likely reason for the refusal; it was not reproduced.
- Only the tour's runs are restricted by BPR, to a read-only tool list
  ([agent_launch.hpp](../../src/application/include/bpr/application/agent_launch.hpp)).
- The agent's working directory is its workspace: a disposable checkout of
  the pull request's head with no remote, or a checkout the reviewer chose
  ([agent_workspace.cpp](../../src/application/src/agent_workspace.cpp),
  [agent_invocation_host.cpp](../../src/application/src/agent_invocation_host.cpp)).
- Per-agent settings live on `agent_entry`
  ([agent_entry.hpp](../../src/core/include/bpr/core/agent_entry.hpp)):
  model, effort, arguments and the run-check options.

RFC-BPR-0011 requires BPR to "Preserve each agent's own model, GitHub, AWS,
sandbox, approval, and credential boundaries". RFC-BPR-0060 forbids
claiming a sandbox that same-user checks do not provide, and holds that
untrusted content cannot execute code.

## Proposed behavior and boundaries

### The scratch folder

Each agent session can have a **scratch folder**: a directory BPR creates
for that session in its own application data, outside the repository and
outside the agent's workspace. When it is on:

- The agent is told the folder's path and that it may create, change and run
  files there, and nowhere else.
- BPR asks the agent's own permission system to allow exactly that: writes
  confined to the folder, and execution of files in it. The workspace and
  every other path remain as the reviewer's own agent configuration has
  them. BPR passes no broader allowance.
- A script run from the folder can read the workspace. That is its purpose.
- The folder lives and dies with the session's workspace under
  RFC-BPR-0096, counts toward the same storage bound, and is limited to 256
  MiB; a write past the limit is refused and the agent is told why.
- The reviewer can open the folder from the session, and a turn's folded
  activity line ([RFC-BPR-0103](RFC-BPR-0103-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md))
  lists the files written and the commands run.

### On to begin with, and remembered per pull request

The owner settled this on 2026-11-10: "Should start on, can have that
initial state be a setting in the agent settings. And should remember your
setting as you change it within a PR."

- Each agent's settings hold **Scratch folder starts** with the values On
  and Off, beside its model and effort. It ships On, for a newly added agent
  and for an agent carried over from an earlier version.
- In a pull request, the reviewer turns the scratch folder off or on from
  the grant line in the Agent tab. The change takes effect at the next turn;
  a running turn keeps the grant it was launched with.
- BPR remembers that choice for that pull request and that agent, through
  New session, closing the pull request and restarting the application.
  Another pull request is not affected, and neither is the agent's starting
  state.
- A pull request where the reviewer has not changed it follows the agent's
  starting state, including a later change to that state.

### Builds and tests

**Build and run tests** is a second, separate setting, off until the
reviewer turns it on in the agent's settings. When it is on for the
agent, and the agent's workspace is the disposable checkout, the agent may
run build and test commands in that workspace and write build output there.
It is never available for a checkout the reviewer chose: BPR does not let an
agent write into the reviewer's own working tree. The setting shows as
unavailable there, with that reason.

Building runs the pull request's own code. The first time in each pull
request, BPR asks: "Building runs this pull request's code on this computer
with your account's rights. Allow for this pull request?" Declining leaves
the agent reading and scripting only.

### Saying what is granted, and what is not

- The Agent tab states the current grant in one line beside the agent pill:
  "Reads only", "Writes only in its scratch folder", "Builds and tests in
  its disposable checkout". The line is always shown, and it is the control
  that turns the scratch folder off or on for this pull request.
- Because the scratch folder is on to begin with, the line and the agent's
  run details are where the reviewer learns that a script the agent writes
  runs with the reviewer's rights.
- BPR's wording never calls this a sandbox. Where the agent's own runtime
  enforces a real boundary (Codex's workspace-write sandbox confined to the
  folder, with network off), the agent's run details say so. Where it does
  not, they say the grant is a permission rule the agent follows, and that a
  script runs with the reviewer's rights.
- Network access is never granted by BPR. It stays as the agent's own
  configuration has it.

### Boundaries

- Tour runs stay read-only. The one-shot hooks stay tool-free.
- Nothing here writes to GitHub or to a tracked file of the reviewer's
  repository.
- This amends RFC-BPR-0011 in one respect: BPR may pass the two narrow
  allowances above when they are on for that agent and pull request. Every
  other boundary is preserved.

### Settled choices

**Builds belong in 0.5.0.** Asked on 2026-11-10 whether opt-in builds and
tests should be part of this release, as proposed above (off until turned on
for an agent, in the disposable checkout only, with the question once per
pull request), the owner answered "Include builds". The alternative was to
ship the scratch folder alone.

**The scratch folder starts on.** This draft had proposed off until turned
on, because a script runs with the reviewer's rights; the owner chose on,
with the starting state as an agent setting and the per-pull-request memory
above.

## Acceptance criteria

- **AC01:** With the scratch folder on, an installed Claude and an installed
  Codex each write a script into the folder, run it against the workspace
  and report its output in one turn; with it off, the launch carries no such
  allowance.
- **AC02:** With the scratch folder on, an attempt by the agent to create or
  change a file in the workspace or elsewhere outside the folder is not
  allowed by anything BPR passed, and `git status` in the workspace is clean
  after the turn.
- **AC03:** The folder is outside the repository and the workspace, is
  removed with its session, and refuses a write past 256 MiB with a reason
  the agent receives.
- **AC04:** Build and run tests is unavailable, with its reason, for a
  checkout the reviewer chose. In a disposable checkout it asks once per
  pull request, and declining leaves the launch without the build allowance.
- **AC05:** The Agent tab's grant line matches the launch for each of the
  three states, and no text calls the grant a sandbox for a runtime that
  does not enforce one.
- **AC06:** Tour runs and one-shot hooks launch exactly as before.
- **AC07:** An independent critical review has read the launch allowances
  for both agents and recorded what each runtime enforces and what it only
  permits.
- **AC08:** A newly added agent and an agent carried over from 0.4.2 both
  launch with the scratch allowance; after the agent's starting state is set
  to Off, a pull request the reviewer has not changed launches without it.
- **AC09:** Turning the scratch folder off from the grant line in one pull
  request holds for that pull request and agent through New session and a
  restart, takes effect at the next turn, leaves another pull request on,
  and leaves the agent's starting state unchanged.
