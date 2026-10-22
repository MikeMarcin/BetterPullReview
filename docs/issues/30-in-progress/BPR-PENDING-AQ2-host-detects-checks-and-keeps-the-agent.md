---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ2"
title: "The host detects the agents when setup shows, checks the chosen one and keeps the choice"
status: "In Progress"
rfcs: ["RFC-BPR-0091"]
depends_on: ["BPR-PENDING-AQ1"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0091 adoption, 2026-10-22 (owner answers Q1-Q9)","related_cards":["BPR-PENDING-AN"],"milestone":"RFC-BPR-0091 M2"}
---

# BPR-PENDING-AQ2: The host detects the agents when setup shows, checks the chosen one and keeps the choice

RFC-BPR-0091 M2 ([RFC](../../rfcs/RFC-BPR-0091-agent-first-run-setup.md#milestones)).

## Scope

- Detection when setup shows and on Look again (Q2): file metadata only, no
  process, nothing written.
- The agent row of Check it works: the agent's status command and a
  `--version` read through the supervised launcher, bounded at 15 seconds,
  Stop-able, with the profile's allowlist and no review context (Q3 A).
- The chosen agent saved for the application profile, and Send to Agent's
  default.

## Acceptance criteria

- AC1: Showing setup starts no process (launcher probe unchanged) (RFC AC01,
  host share).
- AC2: The check's passed, not-signed-in and failed-to-start results reach the
  setup state from fixture agents, and Stop and the bound end it (RFC AC10,
  agent row).
- AC3: The saved agent survives a new window and a restart, and Send to Agent
  offers it first (RFC AC11, agent).
