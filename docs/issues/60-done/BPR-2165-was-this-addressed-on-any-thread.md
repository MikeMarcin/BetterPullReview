---
schema: "implement-rfc.card.v1"
id: "BPR-2165"
title: "Was this addressed? on any thread, a file or the pull request, with a claim chip"
status: "Done"
rfcs: ["RFC-BPR-0128"]
depends_on: ["BPR-2164"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2163--BPR-2168--6bc64f9acd90.json","milestone_plan":{"id":"ask-v1","path":"docs/rfcs/plans/ask-v1/milestone-plan.json"},"criteria":["AC05"],"origin":"imported 2026-11-13 from the owner's Ask design (docs/design/ask-v1); cards cut at import"}
acceptance: [{"criterion":"AC05","outcome":"pass","revision":"612194781","evidence":{"kind":"command","detail":"application_ask_addressed_public_path, application_returning_review_addressed_public_path; owner Accept (BPR-2165-claim-chips) in look ask-v002"}}]
---

# BPR-2165: Was this addressed? on any thread, a file or the pull request, with a claim chip

Part of [RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md); milestone `addressed-anywhere` of the [plan](../../rfcs/plans/ask-v1/MILESTONES.md).

## Scope

RFC-BPR-0116 widened: offered on any review thread, whoever started it; at a file or pull-request scope one run a thread (Q5), the baseline the commit the thread started on (Q6); the claim as one chip in the thread's header in ordinary roles (Q14) with the evidence over the code on press; nothing resolved or posted.

## Acceptance

- **AC05:** Was this addressed? runs on a thread I did not comment in, on each thread of a file and on every thread of the pull request; each claim appears as a chip in its thread's header without changing the drawer's layout height, its press shows the thread's lines at its baseline and now, and nothing is resolved, posted or published.

## Notes

Ends in an owner look (Accept or Reject).

## Progress (2026-11-14)

Frontend-design was loaded first. Built; see the
verification record (`docs/verification/RFC-BPR-0128.md` on lane `claude/v0-5-0-prerelease-targets-52265d`).

- **AC05** passes on the composed application
  (`application_ask_addressed_public_path`, scoped writing stub): offered on
  tbutler's line-6 thread (not mine); the file's row runs once a thread (two
  runs, two claims); the pull request's row reads five threads; README.md's
  thread, started on the head, is greyed "No commits since this thread
  started."; the chip in the open thread's header ("Partly"), the closed
  cards' heights and the header's height unchanged; a press shows the lines as
  the thread started and now; nothing resolved, posted or published.
  `application_returning_review_addressed_public_path` follows the widened
  offer.
- **Q5 (assumption):** more than five runs ask first; put to the owner.
- **Live installed-agent run** passes (Claude Code 2.1.319, at `7b3fe6eaa`).
- **Not claimed:** it depends on [[BPR-2164]] being Done, which is parked on
  the owner's look; the work is done on this lane and the look is filed with
  it. Claim and finish it once BPR-2164 is Done.

## Owner look ask-v002 (2026-11-14)

`BPR-2165-claim-chips` **Accept** (frozen from `612194781`). Claimed and
finished once BPR-2164 was Done. Q5 (the threshold) was not decided in that
look ("cant tell missing mock agent and response."); it is asked again in
ask-v003, now that the pull request's row reaches six threads.
