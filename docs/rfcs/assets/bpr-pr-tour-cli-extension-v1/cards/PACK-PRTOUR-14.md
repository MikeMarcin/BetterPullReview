---
schema: "implement-rfc.card.v1"
id: "PACK-PRTOUR-14"
title: "Implement the thin pr-tour conversational skill"
status: "Backlog"
rfcs: ["RFC-BPR-0046"]
depends_on: ["PACK-PRTOUR-13"]
metadata: {"pack": "bpr-pr-tour-cli-extension-v1", "milestone": "pr-tour-skill", "acceptance": ["AC46-02", "AC46-03", "AC46-04"]}
---

# Implement the thin pr-tour conversational skill

## Outcome

Adapt the candidate SKILL.md and progressive command/interaction/recovery references to the verified CLI. Use explicit IDs and known user answers, persist edits, keep manual first, guide one stop at a time and use active-agent hooks. No standalone draft/publisher/API or direct database mutation.

## Existing ownership seams

skill-source/pr-tour candidate; actual canonical AgentSkills package conventions; CLI help/capability contract.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Static capability/syntax lint and scripted command traces show a small wrapper, no direct gh/http/DB publisher, no hidden full-tour generation, and useful missing/incompatible CLI refusal. Trace tests do not count as model behavior evidence.

Acceptance ownership: AC46-02, AC46-03, AC46-04

## Execution boundary

Milestone: **pr-tour-skill**. This is a pack-local alias, not an allocated live BPR issue. Allocate/remap only extension cards at intake, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
