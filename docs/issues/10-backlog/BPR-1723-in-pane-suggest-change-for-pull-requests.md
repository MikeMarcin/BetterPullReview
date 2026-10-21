---
schema: "implement-rfc.card.v1"
id: "BPR-1723"
title: "Suggest change edits the head file in place in pull request review"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1723--BPR-1723--40864f3da1f3.json","kind":"ux","origin":"owner report 2026-10-21 (relayed by the coordinator)","owner_note":"It seems like the suggest change logic from the local project where we can type our change into the main code pain doesnt exist for the pr review ux","draft_rfc":"docs/rfcs/assets/pr-in-pane-suggestions-v1/RFC-DRAFT-0025-in-pane-suggest-change-for-pull-requests.md","design_frames":"docs/design/BPR-1723-in-pane-suggest-v1/index.html","frontend_design":"frontend-design:frontend-design loaded before the design frames","related_cards":["BPR-1156","BPR-1212","BPR-1721","BPR-1084","BPR-1716","BPR-1706"],"related_rfcs":["RFC-BPR-0055","RFC-BPR-0022","RFC-BPR-0048","RFC-BPR-0089"],"phase":"1 (design); no product code changed"}
---

# BPR-1723: Suggest change edits the head file in place in pull request review

## Owner report

The owner, 2026-10-21, verbatim:

> It seems like the suggest change logic from the local project where we can
> type our change into the main code pain doesnt exist for the pr review ux

## Phase 1 finding

The report is accurate, and no regression caused it. [[BPR-1212]]'s Q2
("B: whole-file editor") kept the in-place editor for local projects and
gave pull requests a ```` ```suggestion ```` block in the drawer
([[BPR-1212]], [[BPR-1721]]). The two sides, the GitHub constraints and the
proposed design are in the draft RFC
[RFC-DRAFT-0025](../../rfcs/assets/pr-in-pane-suggestions-v1/RFC-DRAFT-0025-in-pane-suggest-change-for-pull-requests.md).
The [design frames](../../design/BPR-1723-in-pane-suggest-v1/index.html)
show the current state and each open question. They are design evidence, not
the product.

## Plan

The work is RFC-DRAFT-0025's milestones M1 to M4: core planning, the Qt pane,
the drawer and lifecycle, then the live and attended checks. Each becomes its
own card when the RFC is adopted and IDs are allocated. Q1 to Q5 must be
answered first, because each changes the scope of M1 to M3.

## Owner questions

These are asked in a fresh, short attended-review session, with one question
per decision. Its URL is kept in the external session state.

- **Session.** `e5387830d18edd8d541b96e47ecbd9cb`, with its state in
  `/Users/jaeger/.mask/attended-review/owner-review-20261021-in-pane-suggest-v001`.
  It opened 2026-10-21, and all five candidates were ready at event cursor 15.
  Its loopback host was started for the owner.
- **Candidates**, all at revision `653e2ff2d`, each showing its design frame:
  - `BPR-1723-q1-extent-v001`: Accept or Reject.
  - `BPR-1723-q2-words-v001`: A or B.
  - `BPR-1723-q3-unplaceable-v001`: A or B.
  - `BPR-1723-q4-one-pane-v001`: A or B.
  - `BPR-1723-q5-drawer-block-v001`: Accept or Reject.
- **Status.** No answer has been recorded. The next engineering round reads
  `status --after 15`, records the answers here and in the RFC, and revises
  the RFC to match.

## Acceptance

The RFC's AC01 to AC10 govern. This card's own criterion: the owner's answers
to Q1 to Q5 are recorded here and in the RFC, and the RFC is adopted, revised
or set aside to match.
