---
schema: "implement-rfc.card.v1"
id: "BPR-2009"
title: "Verify and close RFC-BPR-0104"
status: "Backlog"
rfcs: ["RFC-BPR-0104"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0021-close-agent-scratch.md","draft_title":"Close the agent scratch folder RFC"}
depends_on: ["BPR-2007", "BPR-2008"]
closeout_for: ["RFC-BPR-0104"]
---

# BPR-2009: Verify and close RFC-BPR-0104

## Scope and acceptance

Judge every acceptance criterion of
[RFC-BPR-0104](../../rfcs/RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md)
against actual evidence from the cards this one depends on, including its
live, installed-agent and owner-attended criteria, the reconciliation of the
live RFCs it amends, and the documentation gate. Record failures and unreached
requirements exactly.

Leave live IDs, the implementation baseline and acceptance records to
authorized execution. This proposed closeout carries no passing evidence and
changes no live RFC's status.

This is part of `accepted-scope` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Closeout checklist

RFC base: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** With the scratch folder on, an installed Claude and an installed Codex each write a script into the folder, run it against the workspace and report its output in one turn; with it off, the launch carries no such allowance. | Pending | Pending | Not implemented |
| **AC02:** With the scratch folder on, an attempt by the agent to create or change a file in the workspace or elsewhere outside the folder is not allowed by anything BPR passed, and `git status` in the workspace is clean after the turn. | Pending | Pending | Not implemented |
| **AC03:** The folder is outside the repository and the workspace, is removed with its session, and refuses a write past 256 MiB with a reason the agent receives. Amended 2026-11-11: past its set limit (256 MiB to begin with), a turn starting gets no write allowance and is told why, and a turn reaching it is stopped with the reason shown; an unused folder is removed after the kept days, and Settings clears folders. | Pending | Pending | Not implemented |
| **AC04:** Build and run tests is unavailable, with its reason, for a checkout the reviewer chose. In a disposable checkout it asks once per pull request, and declining leaves the launch without the build allowance. | Pending | Pending | Not implemented |
| **AC05:** The footer picker's grant item (amended 2026-11-11, [[BPR-2051]]; its two toggles, amended the same day after owner look `e97c6a3d`) matches the launch for each of the three states, and no text calls the grant a sandbox for a runtime that does not enforce one. | Pending | Pending | Not implemented |
| **AC06:** Tour runs and one-shot hooks launch exactly as before. | Pending | Pending | Not implemented |
| **AC07:** An independent critical review has read the launch allowances for both agents and recorded what each runtime enforces and what it only permits. | Pending | Pending | Not implemented |
| **AC08:** A newly added agent and an agent carried over from 0.4.2 both launch with the scratch allowance; after the agent's starting state is set to Off, a pull request the reviewer has not changed launches without it. | Pending | Pending | Not implemented |
| **AC09:** Turning the scratch folder off from the footer picker's grant item in one pull request holds for that pull request and agent through New session and a restart, takes effect at the next turn, leaves another pull request on, and leaves the agent's starting state unchanged. | Pending | Pending | Not implemented |
