---
schema: "implement-rfc.card.v1"
id: "BPR-1992"
title: "Choose the publication form, advance its steps and report failures"
status: "Parked"
rfcs: ["RFC-BPR-0099"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0004-tour-publication-form-and-steps.md"}
depends_on: ["BPR-1991"]
parked_on: "human-review"
acceptance: [{"criterion":"RFC-BPR-0099 AC03","outcome":"pass","revision":"3d97ea5487952fd6d0872dff72966ac75a14f256","evidence":{"kind":"command","command":"ctest --preset dev -R 'application_review_tour_publication_(native|mixed|file|compat|repairs)|qt_review_tour_publication_working'","detail":"No mode list. Fixture tours choose one review (native), a mixed tour with a conversation comment (mixed) or a file comment (file), and compatibility comments (compat, and file while no file comment is offered); each panel sentence names the stops that forced the form; Keep it to one review instead lists them with their moves and changes nothing."}},{"criterion":"RFC-BPR-0099 AC04","outcome":"pass","revision":"3d97ea5487952fd6d0872dff72966ac75a14f256","evidence":{"kind":"command","command":"ctest --preset dev -R 'review_submission_application_host|qt_review_tour_publication_working|application_review_tour_publication_(stop|native|compat)'","detail":"Steps are the plan's writes; a step is sent only once the record holds GitHub's confirmation; waiting, sending, sent in order with the head's count equal to the panel's at every step; with the first write held, step 1 is sending and nothing is sent."}},{"criterion":"RFC-BPR-0099 AC05","outcome":"pass","revision":"3d97ea5487952fd6d0872dff72966ac75a14f256","evidence":{"kind":"command","command":"ctest --preset dev -R 'qt_review_tour_publication_working|application_review_tour_publication_(stop|conflict|description)'","detail":"The banner names the step, the reason, what is and is not on GitHub, Resume primary; the failed step carries the provider's short reason; nothing in the failure is in the placeholder colour; restart, conflict and description failures are banners."}},{"criterion":"RFC-BPR-0099 AC06","outcome":"pass","revision":"3d97ea5487952fd6d0872dff72966ac75a14f256","evidence":{"kind":"command","command":"ctest --preset dev -R application_review_tour_publication_unsupported","detail":"Unqualified connection: refused at preview, before any write, naming stops 1 and 2, the operation (review comments on their lines) and both ways out (Move those stops, Use another connection); zero writes."}},{"criterion":"RFC-BPR-0099 AC08","outcome":"pending","revision":"3d97ea5487952fd6d0872dff72966ac75a14f256","evidence":{"kind":"freeform","detail":"Live mixed tour on MikeMarcin/BPR-Test not run: a tests-on build holds no publication grant, freeze_attended_build.py freezes only tests-on builds, and the bpr tour CLI publishes only after a person types publish. Needs a tests-off build the owner runs (the choice RFC-BPR-0105 AC08 also waits on). Fixture mixed and file courses meet the exact write assertions written for the hand-chosen mixed mode."}}]
---

# Choose the publication form, advance its steps and report failures

## Scope and acceptance

Implements
[RFC-BPR-0099](../../rfcs/RFC-BPR-0099-tour-publishing-in-reach-and-reported.md).

Remove the mode list; have the plan choose the first form that carries every
stop and state the choice with the stops that forced it; give each publication
step a state in the core's projection and show the advancing checklist in the
panel and the count in the head; replace status-line refusals and failures
with the error-role banner the RFC defines, including the before-sending
refusal that names stops, operation and both ways out.

Covers AC03 to AC06 and the live course of AC08 on MikeMarcin/BPR-Test.
Publication semantics and recovery under RFC-BPR-0040 to 0042 do not change.

This is part of `tour-publishing` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Record

Built in `3d97ea548`; record in
[RFC-BPR-0099 verification](../../verification/RFC-BPR-0099.md). The
application tries one review, a mixed tour and compatibility comments with
the existing planner and says the choice; off-hunk stops become file comments
when the connection's facts allow one, without a per-stop request. Steps are
the plan's writes; the host hands the projection the record it authorised
before each write, and a step is sent only once the record holds GitHub's
confirmation. Failures, stops, restarts with a part-sent record and refusals
are error-role banners; Resume previews the captured plan first. AC03 to AC06
pass on fixture courses. AC08, the live mixed tour on BPR-Test, is pending: a
tests-on build cannot publish and the frozen-build tool freezes only tests-on
builds, so it needs a tests-off build the owner runs. Publication semantics,
recovery and the CLI's explicit choice are unchanged.

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
