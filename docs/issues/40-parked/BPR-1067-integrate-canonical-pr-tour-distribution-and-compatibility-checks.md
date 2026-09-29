---
schema: "implement-rfc.card.v1"
id: "BPR-1067"
title: "Integrate canonical pr-tour distribution and compatibility checks"
status: "Parked"
rfcs: ["RFC-BPR-0046"]
depends_on: ["BPR-1065"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-16","milestone":"pr-tour-skill","acceptance":["AC46-01"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"AC46-01","outcome":"pending","revision":"75f2933b0bb91a2455b0c5087e902e928c9d27db","evidence":{"kind":"observation","detail":"Verified: package, directory and frontmatter name are exactly pr-tour; contract and capability checks work against the real CLI; no divergent hand-maintained copy exists and the project selects the skill only from the shared source (skill_pr_tour_package). Candidate digest sha256:a06be62c08c3fc4de65cdd834d8888e079d9a2ee29524c4ed3b72483083ae2a6. Pending: the canonical import into the AgentSkills repository and its MASK selection, which need that repository's owner's authorization under its own contribution contract; no installation or cross-agent host load is claimed."}}]
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
