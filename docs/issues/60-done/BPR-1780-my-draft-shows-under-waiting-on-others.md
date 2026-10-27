---
schema: "implement-rfc.card.v1"
id: "BPR-1780"
title: "My draft pull request shows under Waiting on others"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1780--BPR-1780--a077d4c25e8a.json","kind":"defect","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0067"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"06adaacddcec3d8a3808f9ecf87f07c232137be3","evidence":{"kind":"command","detail":"core_review_queue_attention test_my_draft_waits_on_nobody: an authored draft with reviewers awaited and a required review is quiet and in My pull requests; ready, it is authored_awaiting_reviewer_action; unknown draft status is unchanged; unseen comments still outrank."}},{"criterion":"AC2","outcome":"pass","revision":"06adaacddcec3d8a3808f9ecf87f07c232137be3","evidence":{"kind":"command","detail":"application_review_queue_enrollment test_my_draft_is_not_waiting_on_others: through discovery, my draft with two requested reviewers is in My pull requests, and after a refresh reporting it ready it is in Waiting on others."}}]
---

# BPR-1780: My draft pull request shows under Waiting on others

## Observed

The owner, 2026-10-27, verbatim:

> On the home I have a draft of a PR that I've authored and it shows up
> under waiting for others. I don't think a draft PR of my PRS can ever be
> waiting for others until it's not a draft

## Cause

`core::bucket_for` (`src/core/src/review_queue_attention.cpp`) put an
authored pull request in `authored_awaiting_reviewer_action` whenever
reviewers were awaited or a required review was pending, and
`home_section_for` shows that bucket as Waiting on others. The draft status
([RFC-BPR-0067](../../rfcs/RFC-BPR-0067-home-draft-pull-requests.md)) was on
the item's facts but not in the activity the bucket reads, so a draft with
reviewers named (GitHub allows requesting them on a draft) was waiting on
others.

## Fix

`queue_activity_observation::Draft` carries the facts' draft status into the
bucket; the three places that compute a bucket (enrollment's projection and
observed item, and the composition's notification check) set it from the
item's facts. The authored rule skips a draft, which then falls to quiet and
shows in My pull requests. Unseen comments and the other reviewer-facing
buckets still outrank it; an unknown status changes nothing.

## Not verified

The owner's own Home with their draft (needs a build with this commit).
Composed Home and Qt Home tests were not rerun; the presentation reads the
same bucket.

## Regression found later, 2026-10-27

`application_personal_discovery_public_path` and
`application_home_waiting_public_path` (and their CLI variants) were not run
at close. Their fixture's authored #4001 was a draft that the courses expected
in Waiting on others, which is the behavior this card removed. The courses
now start #4001 ready (it waits on its reviewers) and turn it into a draft at
the explicit Refresh, after which it is in My pull requests on both windows
and not in Waiting on others. All four pass.
