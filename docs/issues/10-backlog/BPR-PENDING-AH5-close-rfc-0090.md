---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AH5"
title: "Close RFC-BPR-0090 (provisional closeout)"
status: "Backlog"
rfcs: ["RFC-BPR-0090"]
depends_on: ["BPR-PENDING-AH1", "BPR-PENDING-AH2", "BPR-PENDING-AH3", "BPR-PENDING-AH4", "BPR-PENDING-AH8"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0090 adoption, 2026-10-21 (owner answers in session e5387830d18edd8d541b96e47ecbd9cb)","related_cards":["BPR-1723"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0090's design frames v2","milestone":"RFC-BPR-0090 closeout"}
---

# BPR-PENDING-AH5: Close RFC-BPR-0090 (provisional closeout)

The closeout card for [RFC-BPR-0090](../../rfcs/RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md). When its ID is allocated it is
renamed to `BPR-<n>-close-rfc-0090.md`, titled "Verify and close RFC-BPR-0090",
given `closeout_for`, the RFC base and the closeout checklist covering
AC01 to AC10. The provisional name keeps the documentation gate's closeout
rule from reading it before then.

## Acceptance criteria

- AC1: Every RFC-BPR-0090 criterion is recorded with its check, result and
  commit, and the RFC's status is set to match.
