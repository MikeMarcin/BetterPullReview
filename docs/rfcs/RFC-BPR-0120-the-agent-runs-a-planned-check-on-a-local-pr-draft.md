---
id: "RFC-BPR-0120"
title: "The agent runs a planned check on a local PR draft"
status: "Proposed"
metadata: {"origin":"imported 2026-11-10 from the agent-assistance-v1 pack, where it was RFC-DRAFT-0005; rewritten 2026-11-15 without evidence at the owner's direction (\"Rewrite without evidence\")","inspected_revision":"775bd2d764e1651750d06e2ea798ea4d66934996","milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"baseline":"775bd2d764e1651750d06e2ea798ea4d66934996"}
depends_on: ["RFC-BPR-0101", "RFC-BPR-0103", "RFC-BPR-0104", "RFC-BPR-0107", "RFC-BPR-0108", "RFC-BPR-0110", "RFC-BPR-0128"]
---

# The agent runs a planned check on a local PR draft

## Rewritten without evidence (owner, 2026-11-15)

The first version of this RFC ran a check out of an evidence record and
filed the result back into it. The owner rejected that evidence design ("that
seemed like useless"; of its look, "I have no idea what I'm looking at here.
This doesn't seem useful, unless you can explain how it's valuable to me")
and, asked what to do with this RFC, answered "Rewrite without evidence".
This text replaces the earlier one entirely and takes nothing from the
rejected design: there is no evidence record, no claim, no citation, and no
reported-versus-observed classification. The earlier text and criteria are
in Git history; [BPR-2082](../issues/60-done/BPR-2082-run-a-planned-check-with-the-agent.md)
stays closed as rejected.

## Why this is worth having

Before you open a pull request you want to know your change builds and its
tests pass. Today you find out from CI after the pull request is public, or
you stop writing and run things yourself. With this, you ask the agent from
the draft, in plain words ("run the parser tests", "check it still builds
without the optional feature") or with no words at all. It tells you which
commands it will run, and you press **Run**. It runs them in a throwaway copy
of exactly the change you are about to publish, so you keep working and your
folder, branch and editor are never touched. When it finishes you get one
page, read like any other agent report: Passed or Failed, the commands it
ran, the lines of output that matter with links into your code, and what it
did not check. If it failed, you ask about the failure in the same
conversation. Nothing is recorded, scored or published; the page is the
answer.

## Problem and grounded current behavior

- A local draft's agent conversations run tool-free: one request composed
  from the captured candidate, no tools, an empty folder, so the agent
  cannot run anything
  ([RFC-BPR-0110](RFC-BPR-0110-local-tours-and-description.md#implementation-record),
  `compose_local_draft_agent` in
  [window_tours.cpp](../../src/app/src/github_application/window_tours.cpp)).
  The candidate workspace RFC-BPR-0110 describes is not built.
- In a pull request review an agent can already write scripts in its scratch
  folder and, with **Allow build and run tests** on, build and test in its
  disposable checkout, a copy materialized from an immutable revision so
  nothing the reviewer does afterwards affects it
  ([RFC-BPR-0104](RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md),
  [agent_workspace.cpp](../../src/application/src/agent_workspace.cpp)).
  BPR never lets an agent write into the user's own working tree.
- A long agent answer is read as a **report** in a file tab, opened from a
  report card where it was asked
  ([RFC-BPR-0128](RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md);
  the report kind of `document_tabs.hpp`, `report_page.hpp`).
- The owner's original idea was that the agent "proposes checks for a local
  PR draft, runs them" ([note 2](assets/agent-assistance-v1/SOURCE_NOTES.md)),
  held to the rule that "every agent output lands as something the
  application already has".

So the parts exist (a disposable copy, the build grant, the report tab), but
nothing puts them together for a local draft, and nothing runs a check there.

## Proposed behavior and boundaries

### Plan, then run

- The draft's whole-change conversation (the Tour panel's chat on the whole
  change) offers **Plan a check…**, an agent control in the agent accent of
  [RFC-BPR-0101](RFC-BPR-0101-agent-chat-conventions.md). It takes the
  author's words, or none: an empty request asks the agent to choose the
  checks that matter most for this change.
- The agent first reads the check copy (below) and answers with a **plan
  card**: the commands in order, one line each on what it shows, which of
  the two grants it needs, and what the plan will not cover. Nothing has run
  yet. Reading for the plan uses read-only tools.
- The plan card has **Run** and **Discard**. Run is unavailable, with the
  reason, while a grant the plan needs is off (naming the toggle), while
  another check is running in this draft, or while the draft has no
  candidate. To change a plan the author says so in the conversation and the
  agent plans again.
- Run starts the check: the agent is told to run that plan. A running check
  shows its commands as
  [RFC-BPR-0103](RFC-BPR-0103-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md)
  defines and ends in one of its three ways; **Stop** ends it.
- Only the author starts a plan or a run. Nothing runs on open, on refresh,
  on a new candidate or on a schedule, and there is no "check everything".

### The check copy

- A check runs in a **check copy**: a disposable checkout of the draft's
  current candidate, its committed head plus the captured bytes of its
  selected uncommitted files, materialized the way a pull request's agent
  workspace is. It is never the author's worktree, and the live worktree is
  never copied.
- The agent's scratch folder and **Allow build and run tests** apply to the
  check copy as RFC-BPR-0104 applies them to a disposable checkout: the two
  toggles are remembered for this local draft and agent, building is off
  until turned on, and the first build in a draft asks "Building runs this
  change's code on this computer with your account's rights. Allow for this
  local draft?" Network access stays as the agent's own configuration has it,
  and BPR's wording never calls the copy a sandbox.
- The author can edit, save, refresh and capture a new candidate while a
  check runs; the check keeps its own copy and candidate.
- Check copies count toward RFC-BPR-0104's storage limit and are cleaned up
  with it; discarding the draft removes them.

### The report

- A finished, failed or stopped check ends as a **report card** in the
  whole-change conversation, and the card opens the **report** as a file tab,
  as RFC-BPR-0128 defines both.
- The report leads with one outcome word, **Passed**, **Failed**, **Couldn't
  tell**, **Didn't run** or **Stopped**, stated as the agent's report of its
  run, and the candidate it checked ("candidate 3"). Below it: every command
  run, with its exit status where the agent's activity carries one, and any
  command not in the plan marked so; the output lines that explain a failure;
  and what the check did not cover. A code reference opens the candidate at
  that line, as the draft's other links do.
- A check that changed files Git tracks in its copy lists them: the change
  was not tested as written. The copy, and those changes, go with it.
- When the draft has a newer candidate than the one checked, the report says
  so ("Checked candidate 3; the draft is now at candidate 4") and offers
  **Run again**, which runs the same plan on the latest candidate.
- The report is a turn of the conversation: it survives restart with the
  conversation, a later question there reads it, and New session and history
  treat it as any turn. It never enters the description, the tour, the public
  preview or GitHub, and running a check publishes nothing.

### Its prompt

The plan and the run are briefed by one built-in prompt of their own,
`local-draft.run-check` (P02, settled 2026-11-12; see the
[decision record](assets/agent-assistance-v1/DECISIONS.md#decisions)). It is
listed under Built-in prompts › Local drafts, with the editor, default,
version and Reset the others have, and it is an item of the sharing format of
[RFC-BPR-0107](RFC-BPR-0107-agent-and-prompt-sharing.md). Its instructions
reach Claude as an inline `--append-system-prompt` and Codex `exec` as
`developer_instructions`, pasted where unverified or switched off. Each plan
and each run is a fresh run with the tools it needs and no native session;
its card joins the conversation, which carries follow-ups as RFC-BPR-0110
does.

### Not in this RFC

Running in the author's own worktree and fixing a failure (both RFC-BPR-0115's
change runs, if and when they exist; nothing here waits on them); checks on a
pull request review, where the Agent tab already builds and tests; CI; and any
record of results beyond the conversation.

## Owner review of the rewrite (2026-11-15)

The owner accepted this rewrite and every recommended default below, with no
notes, in attended session `ab74a8e490f235b4dd24fa66146a9f14`
(rfc-0120-rewrite-v001, candidate revision `rfc-0120-rewrite-v001-baf674386`,
handed back at event 31; state directory outside the repository under
`~/.attended-review-state/rfc-0120-rewrite-v001-2026-11-15`). That session
judged the design text at `baf674386`; it accepts no built behaviour, and
AC07 stays open for the look on a frozen build.

## Questions answered by the owner (2026-11-15)

Each was accepted at its recommended default, which is now the design.

| # | Question | Answer (accepted default) |
|---|---|---|
| Q1 | Should every check show its plan and wait for Run, or may a check run straight away? | Always plan first: a check runs code with your rights, so you see the commands before they run. |
| Q2 | With no words, does the agent choose which checks to run? | Yes: an empty Plan a check… asks for the checks that matter most for this change, and the plan shows its choice before anything runs. |
| Q3 | Can the author edit the plan's commands before Run? | No: say what to change in the conversation and the agent plans again. An editable command list is a second editor to build and to explain. |
| Q4 | Is one check copy kept per candidate and reused, so a second check reuses the first one's build output? | Yes, one per candidate, removed when a newer candidate is captured and no check is using it. A fresh copy for every check is cleaner but makes each build start cold. |
| Q5 | Where else is a check started? | Only from the whole-change conversation for now. A quick ask in RFC-BPR-0128's Ask menu waits until local drafts have that menu. |

## Acceptance criteria

- **AC01:** Plan a check…, with words or none, returns a plan card naming each command, what it shows and the grants it needs, and nothing executes before Run; Run is unavailable with the specific reason when a needed grant is off, another check is running in the draft, or there is no candidate.
- **AC02:** A run executes in a check copy of the current candidate: the author's worktree, branch and index are unchanged by it, and editing, saving and capturing a new candidate during the run do not change what it checks.
- **AC03:** The report opens as a file tab from its card and shows the outcome word, the candidate checked, each command with its exit status where carried and out-of-plan commands marked, the output explaining a failure, code references that open the candidate at the line, and what was not covered.
- **AC04:** Failed, Couldn't tell, Didn't run and Stopped reports are kept like Passed ones; a check that changed tracked files in its copy lists them; a report on an older candidate says so and Run again checks the latest candidate with the same plan.
- **AC05:** Reports survive restart with the conversation and a follow-up question there reads them; nothing from a check reaches the description, the tour, the public preview or GitHub; check copies count toward the storage limit and are removed with the draft.
- **AC06:** Plans and runs are briefed by `local-draft.run-check`, whose edit reaches the next plan and run and no other prompt, and which round-trips through the sharing format.
- **AC07:** On a frozen build with the fixture agent, the owner accepts, one question each, the plan card, a passing report, a failing report, and a report on an older candidate.

## Proposed delivery

[Plan and run a check, and its report](../issues/20-ready/BPR-2189-plan-and-run-a-check-on-a-local-draft.md)
and [closeout](../issues/10-backlog/BPR-2083-close-rfc-0120.md) form the
fifth [milestone](plans/agent-assistance-v1/milestone-plan.json). The owner
accepted the rewrite on 2026-11-15, and BPR-2189 is Ready.
