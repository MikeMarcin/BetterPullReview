---
schema: "implement-rfc.card.v1"
id: "BPR-2007"
title: "Give an agent session a scratch folder it may write and run in"
status: "Backlog"
rfcs: ["RFC-BPR-0104"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0019-agent-scratch-folder.md"}
depends_on: ["BPR-2005"]
---

# Give an agent session a scratch folder it may write and run in

## Scope and acceptance

Implements
[RFC-BPR-0104](../../rfcs/RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md).

Create the per-session scratch folder outside the repository and the
workspace, bound and remove it with the session, pass each agent's runtime
exactly the confined write and execute allowance when the per-agent setting is
on, tell the agent the path, show the grant line and what each runtime
enforces, and list written files and run commands in the turn's folded
activity line.

Covers AC01 to AC03, AC05, AC06, AC08 and AC09. The owner settled the
default on 2026-11-10: the scratch folder starts on, its starting state is a
per-agent setting, and a change made in a pull request is remembered for that
pull request and agent. Verify each allowance against the installed agents'
own documentation and behaviour rather than from memory.

This is part of `agent-scratch` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
