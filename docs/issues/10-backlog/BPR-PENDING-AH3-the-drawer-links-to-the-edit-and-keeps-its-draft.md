---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AH3"
title: "The drawer links to the in-pane edit, keeps its draft, and converts a suggestion GitHub cannot apply"
status: "Backlog"
rfcs: ["RFC-BPR-0090"]
depends_on: ["BPR-PENDING-AH2"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0090 adoption, 2026-10-21 (owner answers in session e5387830d18edd8d541b96e47ecbd9cb)","related_cards":["BPR-1723"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0090's design frames v2","milestone":"RFC-BPR-0090 M3"}
---

# BPR-PENDING-AH3: The drawer links to the in-pane edit, keeps its draft, and converts a suggestion GitHub cannot apply

RFC-BPR-0090 M3 ([RFC](../../rfcs/RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md#milestones)).

## Scope

- The drawer holds the prose only, with "What it changes"; the draft's body
  is the core's composition and goes through the durable draft path.
- An unplaceable suggestion is marked on the comment and on Finish your
  review, held back, and offered Convert there and on the comment.
- Leaving, reopening, deleting, typing back to the original, a new head and
  Carry, and the composed course on a fixture pull request.

## Acceptance criteria

- AC1: RFC AC04 to AC09 on the composed window against fixtures.
- AC2: Format, tidy on the changed files, the reseals and the documentation
  gate pass.
