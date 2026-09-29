---
schema: "implement-rfc.card.v1"
id: "BPR-1067"
title: "Integrate canonical pr-tour distribution and compatibility checks"
status: "In Progress"
rfcs: ["RFC-BPR-0046"]
depends_on: ["BPR-1065"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-16","milestone":"pr-tour-skill","acceptance":["AC46-01"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
---

# BPR-1067: Integrate canonical pr-tour distribution and compatibility checks

Pack alias: **PACK-PRTOUR-16** (live `BPR-1067`, allocated at import).

## Outcome

Install the skill through the actual shared AgentSkills/MASK source and record source revision, CLI compatibility and selected hosts. Do not overwrite unrelated skills or revive repository-local role implementations. Package a distributable thin skill separate from desktop dependencies under existing license/release policy.

## Existing ownership seams

Canonical shared AgentSkills skills/pr-tour; current MASK source/install metadata; BPR CLI distribution inventory.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Name/frontmatter/directory are pr-tour, required binary/version checks work and no divergent hand-maintained Claude/Codex copies exist. Absent authorized upstream access remains a named distribution blocker, not a fork or false install claim.

Acceptance ownership: AC46-01

## Execution boundary

Milestone: **pr-tour-skill**. Pack alias PACK-PRTOUR-16 was allocated as live `BPR-1067` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
