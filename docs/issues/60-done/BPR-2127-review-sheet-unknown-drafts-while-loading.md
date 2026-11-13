---
schema: "implement-rfc.card.v1"
id: "BPR-2127"
title: "Review sheet says drafts are loading instead of claiming none"
status: "Done"
rfcs: []
depends_on: ["BPR-2054"]
priority: "P1"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2126--BPR-2127--8a16bd6f852c.json","source_commit":"4aa524c2b"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4aa524c2b","evidence":{"kind":"command","detail":"application_start_review_contract: with the draft read held back, the sheet says 'Loading pending comments…', no verdict or Submit is possible, then the same sheet shows 2 pending comments and Comment submits; failed at unread-review-not-said-loading without the core check."}},{"criterion":"AC2","outcome":"pass","revision":"4aa524c2b","evidence":{"kind":"command","detail":"core_review_submission, qt_review_submission_editor_scale_*, application_review_submission_composition_*, architecture_boundaries, format and tidy pass."}}]
---

# Review sheet says drafts are loading instead of claiming none

Owner report: opening Comment or Approve on a reopened pull request before it
loaded said there were no comments, and could submit a review without the
saved drafts. Core now models pending comments as unknown until this window
has read the drafts; no verdict can submit meanwhile.

## Acceptance

- AC1. While drafts load the sheet says so, blocks submitting, and updates to
  the real count once they load.
- AC2. A review never submits without drafts that have not been read.

Not covered: a draft read that never completes keeps saying Loading rather
than naming a failure.
