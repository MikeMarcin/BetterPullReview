---
schema: "implement-rfc.card.v1"
id: "DRAFT-0019"
title: "Give an agent session a scratch folder it may write and run in"
status: "Backlog"
rfcs: ["RFC-DRAFT-0007"]
depends_on: ["DRAFT-0017"]
---

# Give an agent session a scratch folder it may write and run in

## Scope and acceptance

Implements
[RFC-DRAFT-0007](../RFC-DRAFT-0007-agent-scratch-folder-and-opt-in-builds.md).

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

This is part of `agent-scratch` in the [plan](../milestone-plan.json).
