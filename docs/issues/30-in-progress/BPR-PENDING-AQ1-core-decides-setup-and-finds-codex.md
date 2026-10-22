---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ1"
title: "Core decides the agent setup, and a shipped build finds and starts Codex and the plain-text harness"
status: "In Progress"
rfcs: ["RFC-BPR-0091"]
depends_on: ["BPR-PENDING-AN"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0091 adoption, 2026-10-22 (owner answers Q1-Q9)","related_cards":["BPR-PENDING-AN","BPR-1301"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0091's design frames","milestone":"RFC-BPR-0091 M1"}
---

# BPR-PENDING-AQ1: Core decides the agent setup, and a shipped build finds and starts Codex and the plain-text harness

RFC-BPR-0091 M1 ([RFC](../../rfcs/RFC-BPR-0091-agent-first-run-setup.md#milestones)).

## Scope

- The core's agent setup model: the steps, the agent rows (plain names, the
  found path and how it was found, host policy), the choice rule (one found
  is chosen, several wait for the reviewer), the decoded result of the status
  check, and the failure words for each setup reason.
- Codex is found as Claude is (Q5): a saved Codex program path, then `PATH`,
  then the usual folders; on Windows only `codex.exe`. The plain-text harness
  runs only a program path the reviewer chose and is never searched for.
- Every Codex and plain-text-harness launch resolves to an absolute program
  at launch, so a shipped build no longer hands a bare name to a launcher
  that refuses it.

## Acceptance criteria

- AC1: Core tests cover the rows, order, policy refusal, the choice rule,
  the check decoding and every failure's words (RFC AC02, core share; AC13,
  core share).
- AC2: The Codex search order and refusals are tested, and a composed launch
  with no fixture executable override starts the found absolute `codex`;
  none found ends with words naming where it looked (RFC AC03).
- AC3: `core-only` builds and its tests pass; format, tidy on the changed
  files and the documentation gate pass.
