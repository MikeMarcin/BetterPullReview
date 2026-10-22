---
schema: "implement-rfc.card.v1"
id: "BPR-1723"
title: "Suggest change edits the head file in place in pull request review"
status: "Done"
rfcs: ["RFC-BPR-0090"]
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1723--BPR-1723--40864f3da1f3.json","kind":"ux","origin":"owner report 2026-10-21 (relayed by the coordinator)","owner_note":"It seems like the suggest change logic from the local project where we can type our change into the main code pain doesnt exist for the pr review ux","draft_rfc":"docs/rfcs/assets/pr-in-pane-suggestions-v1/RFC-DRAFT-0025-in-pane-suggest-change-for-pull-requests.md","design_frames":"docs/design/BPR-1723-in-pane-suggest-v1/index.html","frontend_design":"frontend-design:frontend-design loaded before the design frames","related_cards":["BPR-1156","BPR-1212","BPR-1721","BPR-1084","BPR-1716","BPR-1706"],"related_rfcs":["RFC-BPR-0055","RFC-BPR-0022","RFC-BPR-0048","RFC-BPR-0089"],"phase":"1 (design); no product code changed","adopted_rfc":"docs/rfcs/RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md","design_frames_v2":"docs/design/BPR-1723-in-pane-suggest-v2/index.html","milestone_cards":["BPR-PENDING-AH1","BPR-PENDING-AH2","BPR-PENDING-AH3","BPR-PENDING-AH4","BPR-PENDING-AH5"],"follow_ups":["BPR-PENDING-AH6","BPR-PENDING-AH7"],"owner_session":{"id":"e5387830d18edd8d541b96e47ecbd9cb","state":"/Users/jaeger/.mask/attended-review/owner-review-20261021-in-pane-suggest-v001","status":"closed 2026-10-21, handed back, host stopped"}}
acceptance: [{"criterion":"The owner's answers to Q1 to Q5 are recorded here and in the RFC, and the RFC is adopted, revised or set aside to match","outcome":"pass","revision":"07a9594c3a64dc9176a97335b04a6cead7b637cc","evidence":{"kind":"observation","detail":"attended_review.py status --after 15 on session e5387830d18edd8d541b96e47ecbd9cb read the five decisions (sequences 17 to 52: Q1 reject superseding an accept, Q2 A, Q3 B with its note, Q4 A with its note, Q5 accept); the coordinator relayed the owner's chat clarification of Q1. All are recorded on this card and in RFC-BPR-0090 r2, which adopts the revised draft. hand-back reported nothing pending, and the host stopped."}}]
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

These were asked in a fresh, short attended-review session, with one question
per decision. Its URL is kept in the external session state.

- **Session.** `e5387830d18edd8d541b96e47ecbd9cb`, with its state in
  `/Users/jaeger/.mask/attended-review/owner-review-20261021-in-pane-suggest-v001`.
  It opened 2026-10-21, and all five candidates were ready at event cursor 15.
- **Candidates**, all at revision `653e2ff2d`, each showing its design frame.
  They were prepared under the provisional names this card had then:
  - `BPR-PENDING-AF-q1-extent-v001`: Accept or Reject.
  - `BPR-PENDING-AF-q2-words-v001`: A or B.
  - `BPR-PENDING-AF-q3-unplaceable-v001`: A or B.
  - `BPR-PENDING-AF-q4-one-pane-v001`: A or B.
  - `BPR-PENDING-AF-q5-drawer-block-v001`: Accept or Reject.
- **Status.** All five answered by event sequence 52 (read with
  `status --after 15`). The session was handed back with nothing pending,
  its candidates' protections released (`resume-development --all`), handed
  back again, and its host stopped through the script's `stop` on
  2026-10-21; host PID 82102 exited with it. The worktree the session was
  bound to had been removed, so the commands ran against a temporary empty
  directory at that path, removed afterwards.

## Owner answers (2026-10-21)

- **Q1, extent: Reject** (sequence 19; an accept at sequence 17 was
  superseded). The note on the rejection did not save. The owner told the
  coordinator in chat: "Only the selected lines are editable in place." So
  Suggest change makes only the selected line range editable in the head
  pane, one suggestion per Suggest change.
- **Q2: A**, each change has its own words (sequence 21).
- **Q3: B, with a note** (sequence 40): "Can we do a mix of both? Mark it and
  give the option to convert it to a file comment say on the finalize review
  page or on the comment itself?" An unplaceable change is marked, and
  Convert to file comment is offered on the Finish your review page and on
  the comment itself.
- **Q4: A**, one pane shows the head file while editing (sequence 50). Note:
  "I kind of want to say 2 pane with top over bottom but I think that's too
  big of a change to land here." A stacked two-pane layout is recorded as
  backlog card BPR-PENDING-AH6.
- **Q5: Accept** (sequence 52). The drawer stops prefilling a suggestion
  block.

## Outcome

The draft was revised to the answers and adopted as
[RFC-BPR-0090](../../rfcs/RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md)
(revision r2; [import record](../../rfcs/assets/pr-in-pane-suggestions-v1.LIVE_IMPORT.md)).
The revised design, in short:

- Only the selected lines are editable. The span runs from the start of the
  first line to the end of the last line's text; an edit that would change
  any other byte is refused whole by the core, and the status line says
  "Only lines a–b are in this suggestion. Esc ends the edit."
- Enter or pasted lines inside the span grow the range and push the rows
  below down; emptying it is a deletion. The comment stays on the selected
  lines however many rows the block holds.
- The range must pass the existing rules: head side, one changed region
  (a single line outside every hunk is refused for a suggestion), and no
  range on a filtered diff whose right side is not head.
- One comment per edit. Frames for this design are in
  [design frames v2](../../design/BPR-1723-in-pane-suggest-v2/index.html).

Its milestones are cards BPR-PENDING-AH1 to AH4, with closeout
BPR-PENDING-AH5 and follow-ups AH6 and AH7.

## Acceptance

The RFC's AC01 to AC10 govern. This card's own criterion: the owner's answers
to Q1 to Q5 are recorded here and in the RFC, and the RFC is adopted, revised
or set aside to match.
