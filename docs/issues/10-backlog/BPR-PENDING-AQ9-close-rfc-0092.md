---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ9"
title: "Close RFC-BPR-0092 (provisional closeout)"
status: "Backlog"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-PENDING-AQ1", "BPR-PENDING-AQ2", "BPR-PENDING-AQ3", "BPR-PENDING-AQ4", "BPR-PENDING-AQ5", "BPR-PENDING-AQ6", "BPR-PENDING-AQ7", "BPR-PENDING-AQ8"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0092 adoption, 2026-10-22","related_cards":["BPR-PENDING-AN"],"milestone":"RFC-BPR-0092 closeout"}
---

# BPR-PENDING-AQ9: Close RFC-BPR-0092 (provisional closeout)

The closeout card for [RFC-BPR-0092](../../rfcs/RFC-BPR-0092-agent-first-run-setup.md). When its ID is allocated it is
renamed to `BPR-<n>-close-rfc-0092.md`, titled "Verify and close RFC-BPR-0092",
given `closeout_for`, the RFC base and the closeout checklist covering
AC01 to AC14. The provisional name keeps the documentation gate's closeout
rule from reading it before then.

## Acceptance criteria

- AC1: Every RFC-BPR-0092 criterion is recorded with its check, result and
  commit, and the RFC's status is set to match.
