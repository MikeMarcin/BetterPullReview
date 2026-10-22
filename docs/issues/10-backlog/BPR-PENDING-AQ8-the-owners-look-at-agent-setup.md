---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ8"
title: "A frozen build and the owner's look at agent setup"
status: "Backlog"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-PENDING-AQ6", "BPR-PENDING-AQ7"]
priority: "P2"
platforms: ["mac"]
metadata: {"origin":"RFC-BPR-0092 adoption, 2026-10-22","related_cards":["BPR-PENDING-AN"],"milestone":"RFC-BPR-0092 M8"}
---

# BPR-PENDING-AQ8: A frozen build and the owner's look at agent setup

RFC-BPR-0092 M8 ([RFC](../../rfcs/RFC-BPR-0092-agent-first-run-setup.md#milestones)).

## Scope

- A frozen build with `scripts/freeze_attended_build.py`, a fresh short
  attended session served with `serve --tailscale`, one question per
  criterion, and the questions also in the report while the page's
  Accept/Reject bug stands.

## Acceptance criteria

- AC1: RFC AC14, the owner's own words recorded with the session.
