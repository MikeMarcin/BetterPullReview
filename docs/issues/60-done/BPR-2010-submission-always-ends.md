---
schema: "implement-rfc.card.v1"
id: "BPR-2010"
title: "Find the stalled submission and make every submission end in a stated condition"
status: "Done"
rfcs: ["RFC-BPR-0105"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0022-submission-always-ends.md"}
depends_on: []
acceptance: [{"criterion":"RFC-BPR-0105 AC01","outcome":"pass","revision":"31f9009103e26314c3ec69cbf15e08807e4ef64b","evidence":{"kind":"artifact","detail":"Baseline course on dc5453afa reproduced both waits behind the shared publication lane; RFC cause section amended; record docs/verification/RFC-BPR-0105.md#ac01-the-stall-reproduced"}},{"criterion":"RFC-BPR-0105 AC02","outcome":"pass","revision":"31f9009103e26314c3ec69cbf15e08807e4ef64b","evidence":{"kind":"command","command":"ctest --preset dev -R application_dependable_session_public_path","detail":"Steps 1 and 2: with the shared lane held, and with #17's own submission held on its lane, #18's Approve is offered and publishes."}},{"criterion":"RFC-BPR-0105 AC03","outcome":"pass","revision":"31f9009103e26314c3ec69cbf15e08807e4ef64b","evidence":{"kind":"command","command":"ctest --preset dev -R application_dependable_session_public_path","detail":"Step 3 (bound shortened to 1.5 s by the course; product constant 20 s): Waiting names its subject, ends Not published 'Still waiting for ...' with Retry, summary intact, nothing sent; Retry publishes once released."}},{"criterion":"RFC-BPR-0105 AC04","outcome":"pass","revision":"31f9009103e26314c3ec69cbf15e08807e4ef64b","evidence":{"kind":"command","command":"ctest --preset dev -R application_dependable_session_public_path","detail":"Step 4: authority changed while the submission waited; its retired host's answer ends the page Not published, never left submitting."}},{"criterion":"RFC-BPR-0105 AC05","outcome":"pass","revision":"31f9009103e26314c3ec69cbf15e08807e4ef64b","evidence":{"kind":"command","command":"ctest --preset dev -R application_dependable_session_public_path","detail":"Step 5: the normal diagnostic log holds review_submit_request, _wait with submit_subject and duration_ms, _send, _drop submit_subject=retired, _end with published and wait_expired."}},{"criterion":"RFC-BPR-0105 AC08","outcome":"pass","revision":"33c282ccd","evidence":{"kind":"command","command":"python3 tests/end_to_end/live_session_course.py (opt-in live course)","detail":"Amended by the owner to an agent-run live course. tests/end_to_end/live_session_course.py --reads 1,2,3,43 --approve 151 --agent /opt/homebrew/bin/claude --cleanup: six pull requests in one window with agent and tour use, then Approve on BPR-Test #151: 'Sending your review to GitHub…', page closed, 'Review submitted. Closed the review of MikeMarcin/BPR-Test#151.'; gh: #151 review 5854726479 by MikeMarcin APPROVED. docs/verification/RFC-BPR-0105.md#agent-run-live-course-2026-11-10"}}]
---

# Find the stalled submission and make every submission end in a stated condition

## Scope and acceptance

Implements
[RFC-BPR-0105](../../rfcs/RFC-BPR-0105-a-review-opens-fresh-and-a-submission-always-ends.md).

Start with the diagnosis the RFC requires: reproduce the stall with a long
multi-pull-request course or record the cause found by other means, and amend
the RFC's cause section if it differs. Then show Waiting, Sending, Published
or Not published for every submission, bound the wait to start at 20 seconds,
end the visible submission when a completion no longer matches, keep another
pull request's effects from blocking this one's submission, and log the
request, waits, drops and ending.

Covers AC01 to AC05 and the owner's course in AC08. Give this path the
repository's highest scrutiny: it publishes under the reviewer's account. Use
fixtures and MikeMarcin/BPR-Test only.

This is part of `dependable-session` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Record

Diagnosis first, as the RFC requires: the public-path course
`application_dependable_session_public_path`, run on the baseline, reproduced
both waits behind the shared per-profile publication lane, and the RFC's cause
section is amended from it ([verification](../../verification/RFC-BPR-0105.md#ac01-the-stall-reproduced)).

Change: each whole-review host sends on its own lane and waits only for a
retired host of its own review identity; the host reports `started` and can
`abandon` work that has not begun; the workflow states Waiting (with its
subject), Sending, Published or Not published, bounds the wait to start at
`core::review_submission_wait_bound`, settles the shown submission's answer
even from a retired host, offers Retry, labels the account line "Will submit
as", and logs `review_submit_request|wait|send|drop|end` with a
`submit_subject`. AC01 to AC05 pass on the course; AC08 is the owner's live
course and is pending.

## Live look v002 abandoned (2026-11-10)

Session `26d89bac1f89acd7841ee451d1e5ffd1` (root `bpr-live-v002.nnhzt2pb`,
frozen from `abba53926`) is abandoned. The build predated the tour-stops
redesign, so it showed the old stop card: its stops could not be accepted and
Publish stayed disabled. The owner: "this is way too many manual steps ... a
freaking mess I cant even accept these comments or try to publish ... Things
are moving around as I click them, buttons packed in like a clown car." The
Accept records `ec9e4113` and `e5e3d58e` from that session are not evidence:
RFC-BPR-0099 AC08 and RFC-BPR-0105 AC08 are **not passed**. Both move to an
agent-run live course on BPR-Test in a later round. BPR-Test is left as is
(the owner pushed `929b028f7` to #152's branch). Host 36810/36892 stopped and
the root removed with `freeze_attended_build.py remove` after the owner's app
(PID 46168) had exited.

## Agent-run live course (2026-11-10)

RFC-BPR-0105 AC08 amended by the owner to an agent-run course; it passes on
lane `33c282ccd`: after six pull requests in one window with agent and tour
use, the course pressed Approve on BPR-Test #151 as MikeMarcin; the page
showed "Sending your review to GitHub…", closed, and the window said "Review
submitted. Closed the review of MikeMarcin/BPR-Test#151."; `gh` reads one
APPROVED review by MikeMarcin (id 5854726479). Record:
[RFC-BPR-0105](../../verification/RFC-BPR-0105.md#agent-run-live-course-2026-11-10).
