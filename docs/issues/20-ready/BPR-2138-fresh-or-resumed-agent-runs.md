---
schema: "implement-rfc.card.v1"
id: "BPR-2138"
title: "Each agent action runs fresh or resumes, as the owner classified them"
status: "Ready"
rfcs: ["RFC-BPR-0101"]
depends_on: []
priority: "P1"
platforms: []
metadata: {"mandate_revision": "v0-5-0-prerelease-targets-r1", "reservation": "docs/issues/_reservations/BPR-2138--BPR-2142--6b47e1d85608.json", "origin": "owner agent launch policy, 2026-11-12 (coordinator brief), corporate wrapper test results and their correction of 2026-11-11", "criteria": ["AC10", "AC11", "AC12"]}
---

# Each agent action runs fresh or resumes, as the owner classified them

## Scope and acceptance

Implements the owner's classification (RFC-BPR-0101, amendment of
2026-11-12): fresh disposable runs for one-offs (Enhance everywhere, a stop's
comment and empty-field drafts, the review-feedback hook, Draft Tour's first
draft, a local draft's first description and commit message, agent checks and
probes, the agent-assistance actions' first answers), resumed native sessions
for conversations (Agent tab, tour side chats and the whole-tour
conversation, the local draft's discussion, Review Cycle rounds, self-review
and fix loops, address-feedback runs).

- The core names every launch path and its session mode; the launch builder
  enforces it.
- A tour stop's Enhance is a fresh run whose result is a proposal on the
  stop; nothing is sent into the stop's side chat.
- Ask more promotes a fresh answer into a resumed session seeded with it.
- A test asserts each launch path's session mode (AC10).

Acceptance: AC10, AC11, AC12 as amended on 2026-11-12.
