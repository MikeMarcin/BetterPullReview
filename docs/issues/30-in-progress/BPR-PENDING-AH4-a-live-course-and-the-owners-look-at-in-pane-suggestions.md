---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AH4"
title: "Publish in-pane suggestions on BPR-Test, measure CRLF, and take the owner's look"
status: "In Progress"
rfcs: ["RFC-BPR-0090"]
depends_on: ["BPR-PENDING-AH3"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0090 adoption, 2026-10-21 (owner answers in session e5387830d18edd8d541b96e47ecbd9cb)","related_cards":["BPR-1723"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0090's design frames v2","milestone":"RFC-BPR-0090 M4"}
---

# BPR-PENDING-AH4: Publish in-pane suggestions on BPR-Test, measure CRLF, and take the owner's look

RFC-BPR-0090 M4 ([RFC](../../rfcs/RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md#milestones)).

## Scope

- A course on `MikeMarcin/BPR-Test`: the core's planned bodies published as
  the composition decides (a tests-enabled build holds no publication
  grant, as RFC-BPR-0089's M4 did), read back, then GitHub's Commit
  suggestion, for an LF file and a CRLF file. Everything made is cleaned up.
- A frozen build and a fresh short attended session, one question per look
  criterion.

## Acceptance criteria

- AC1: RFC AC03 (live), with the CRLF measurement recorded.
- AC2: RFC AC10, from the owner.
