---
schema: "implement-rfc.card.v1"
id: "BPR-2012"
title: "Verify and close RFC-BPR-0105"
status: "Done"
rfcs: ["RFC-BPR-0105"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0024-close-fresh-review-and-submission.md","draft_title":"Close the fresh review and submission RFC"}
depends_on: ["BPR-2010", "BPR-2011"]
closeout_for: ["RFC-BPR-0105"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"357df825deb811adbd45c366172d609abe2e2dc2","evidence":{"kind":"observation","detail":"Baseline course on dc5453afa reproduced the stall; RFC cause amended"}},{"criterion":"AC02","outcome":"pass","revision":"31f9009103e26314c3ec69cbf15e08807e4ef64b","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R application_dependable_session_public_path"}},{"criterion":"AC03","outcome":"pass","revision":"31f9009103e26314c3ec69cbf15e08807e4ef64b","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R application_dependable_session_public_path"}},{"criterion":"AC04","outcome":"pass","revision":"31f9009103e26314c3ec69cbf15e08807e4ef64b","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R application_dependable_session_public_path"}},{"criterion":"AC05","outcome":"pass","revision":"31f9009103e26314c3ec69cbf15e08807e4ef64b","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R application_dependable_session_public_path"}},{"criterion":"AC06","outcome":"pass","revision":"31f9009103e26314c3ec69cbf15e08807e4ef64b","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R application_dependable_session_public_path"}},{"criterion":"AC07","outcome":"pass","revision":"31f9009103e26314c3ec69cbf15e08807e4ef64b","evidence":{"kind":"command","detail":"See the closeout checklist and ledger","command":"ctest --preset dev -R application_dependable_session_public_path"}},{"criterion":"AC08","outcome":"pass","revision":"33c282ccdf87eaee5975fb5e5327c256b9efb01a","evidence":{"kind":"observation","detail":"Agent-run live course: six pull requests with agent and tour use, then Approve on BPR-Test #151; MikeMarcin's APPROVED review 5854726479 read with gh"}}]
---

# BPR-2012: Verify and close RFC-BPR-0105

## Scope and acceptance

Judge every acceptance criterion of [RFC-BPR-0105](../../rfcs/RFC-BPR-0105-a-review-opens-fresh-and-a-submission-always-ends.md) against
actual evidence from the cards this one depends on, including its live,
installed-agent and owner-attended criteria, the reconciliation of the live
RFCs it amends, and the documentation gate. Record failures and unreached
requirements exactly.

Leave live IDs, the implementation baseline and acceptance records to
authorized execution. This proposed closeout carries no passing evidence and
changes no live RFC's status.

This is part of `accepted-scope` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Execution checkpoint

Milestone `dependable-session` engineered at `31f900910`; AC01–AC07 have
public-path evidence; AC08 (amended to an agent-run live course) passed on
`33c282ccd`. Checkpoint:
[handoff](../_checkpoints/v0-5-0-prerelease-targets-v1/handoff.md). No
task-owned command or attended candidate is running.

## Closeout checklist

RFC base: `dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** A recorded course reproduces the stalled submission on the inspected revision, or records the attempts that failed to and the cause found by other means; the RFC's cause section agrees with the record. | Baseline course on dc5453afa; RFC cause amended | Reproduced and recorded | `357df825d` |
| **AC02:** With an earlier effect for a previous pull request held open, a submission for the current pull request is sent and published. | application_dependable_session_public_path steps 1, 2 | Pass | `31f900910` |
| **AC03:** With the current pull request's own submission unable to start, the surface shows Waiting with its reason and, at 20 seconds, Not published with Retry; the draft is intact and Retry submits once the obstruction is gone. | application_dependable_session_public_path step 3 | Pass (bound shortened by the course; constant 20 s) | `31f900910` |
| **AC04:** A completion that arrives for a superseded request leaves the surface in Published or Not published, never in Submitting. | application_dependable_session_public_path step 4 | Pass | `31f900910` |
| **AC05:** The diagnostic log of the blocked-lane, bounded-wait and superseded-completion courses contains the request, each wait with its subject and duration, each drop with its reason, and the ending. | application_dependable_session_public_path step 5 (normal diagnostic log) | Pass | `31f900910` |
| **AC06:** After a review with the Tour tab and a side chat open, opening a different pull request from Home shows the tray on Comments with no side chat, and the same holds when the same pull request is reopened. | application_dependable_session_public_path step 6 | Pass from Home; notification open not exercised | `31f900910` |
| **AC07:** The remembered filter defaults and the window's drawer widths are the same before and after opening the different pull request. | application_dependable_session_public_path step 6 | Pass | `31f900910` |
| **AC08:** A pull request on MikeMarcin/BPR-Test is approved through the window's own Approve after a course of at least five pull requests with agent and tour use, and it publishes. (Amended 2026-11-10 by the owner: it was the owner's own run on a frozen build, which proved "way too many manual steps"; an agent-run live course through the same window and actions replaces it.) | Agent-run live course `tests/end_to_end/live_session_course.py` (six pull requests, agent and tour use, Approve on #151) | Pass: #151 has MikeMarcin's APPROVED review 5854726479 (read with `gh`) | `33c282ccd` |

## Closeout (2026-11-11)

Every criterion judged against the [ledger](../../verification/RFC-BPR-0105.md). AC06 was exercised from Home (the criterion's path); a notification's open was not. BPR-2088 and BPR-2090 (Backlog) are later observations outside these criteria. Documentation gate run before this commit. RFC-BPR-0105 is Auto-Verified.
