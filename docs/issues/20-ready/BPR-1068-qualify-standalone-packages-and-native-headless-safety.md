---
schema: "implement-rfc.card.v1"
id: "BPR-1068"
title: "Qualify standalone packages and native headless safety"
status: "Ready"
rfcs: ["RFC-BPR-0044", "RFC-BPR-0045", "RFC-BPR-0046"]
depends_on: ["BPR-1059", "BPR-1062", "BPR-1063", "BPR-1066", "BPR-1067"]
metadata: {"pack":"bpr-pr-tour-cli-extension-v1","pack_alias":"PACK-PRTOUR-17","milestone":"verification-closeout","acceptance":["AC44-12"],"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
---

# BPR-1068: Qualify standalone packages and native headless safety

Pack alias: **PACK-PRTOUR-17** (live `BPR-1068`, allocated at import).

## Outcome

Prove first-run profile/auth setup without a desktop install, and produce tests-disabled standalone CLI packages using existing target-native lanes and dependency/notices policy. Exercise native console/process/vault/owner channel, editor/temp and output safety. Preserve supported/unsupported transport distinctions rather than equate compiling with service qualification.

## Existing ownership seams

Existing packaging and native platform lanes; CLI/headless CMake target; security/dependency verifiers.

Use the nearest current AGENTS.md and the actual mapped parent contracts. These are integration seams, not permission to copy embedded worktree snapshots or build parallel services.

## Direct acceptance

Record actual Windows/macOS/Linux observations per shipped capability; absent targets stay Not run. No GUI requirement, boot daemon, token extraction, repo code execution or unqualified CLI transport is introduced.

Acceptance ownership: AC44-12

## Execution boundary

Milestone: **verification-closeout**. Pack alias PACK-PRTOUR-17 was allocated as live `BPR-1068` at import, preserving original plan lineage.

Run focused tests and record command/result/producing revision. Reuse unchanged valid parent evidence. An implementation card does not authorize real company PR writes, remote pushes, credentials, paid model calls, agent permission changes or upstream skill installation. Use existing isolated fixtures and separately authorized designated acceptance. Missing capabilities/evidence remain explicit. No GitHub Actions or new model-routing topology is introduced.
