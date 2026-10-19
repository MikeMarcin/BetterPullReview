---
schema: "implement-rfc.card.v1"
id: "BPR-1378"
title: "A tour publication stopped by its own Stop reports conflict"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: []
metadata: {"kind":"defect","found_by":"BPR-1222's two-account course on MikeMarcin/BPR-Test#50, staged_review_session --live-tour-course at 400e6ddc2 on e964b5a83 (2026-10-09)","reservation":"docs/issues/_reservations/BPR-1377--BPR-1378--58ecbdbf1b08.json","related_cards":["BPR-1222","BPR-1368"],"related_rfcs":["RFC-BPR-0041","RFC-BPR-0042"],"milestone_plan":{"id":"bpr1378-tour-stop","path":"docs/rfcs/plans/bpr1378-tour-stop/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4773277f981d429c29862e297c0e31c3d5e90335","evidence":{"kind":"observation","detail":"Fourteen production-session Stop points retain exact confirmed progress; verification-read cancellation preserves prepared steps; real drift remains conflict. See docs/issues/_checkpoints/BPR-1378/verification.md."}},{"criterion":"AC2","outcome":"pass","revision":"4773277f981d429c29862e297c0e31c3d5e90335","evidence":{"kind":"command","detail":"Final focused staged_review_session, staged_review_restart, tour_cli_publication and tour_cli_terminal_publication: 4/4 pass. New state assertions fail against old behavior; exact read-only recovery and Resume totals pass. See docs/issues/_checkpoints/BPR-1378/verification.md."}}]
---

# BPR-1378: a tour publication stopped by its own Stop reports conflict

## Observed

`review_submission_session::publish_tour` returned
`staged_publication_state::conflict` twice on `MikeMarcin/BPR-Test#50`
when the caller's stop token was requested between writes, and nothing on
GitHub conflicted:

- Initial publication stopped after the pending review and its three
  GraphQL stop edits (4 writes): state 4 (`conflict`). The next process
  finished it to `published` with one submit and the description edit.
- Reorder update stopped after its first stop-body PATCH (1 write): state 4.
  A fresh process's read-only `reconcile_tour` of the same plan reported
  `ready` with `Updated = 1`, no write, and `publish_tour` then completed it
  with two PATCHes and the review-body PUT, no POST.

## Cause

In `execute_tour`, a requested stop leads to `verify_staged(current, aStop)`
with the already-stopped token; its reads refuse, and the refusal is
reported as `conflict`. The fixture Stop course in
`staged_review_session_tests.cpp` asserts only write counts, so it does not
see the state.

## Why it matters

A person who presses Stop (or Ctrl-C in `bpr tour publish`) is told the
remote tour conflicts, which invites them to treat the website as changed
under them. The recorded journal and a later read are correct; only the
in-process answer is wrong.

## Acceptance

- A Stop between writes reports a stopped/partial state that names what
  completed, not `conflict`; a real remote drift still reports `conflict`.
- The fixture Stop course asserts the returned state at every stop point.
