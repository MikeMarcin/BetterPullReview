---
schema: "implement-rfc.card.v1"
id: "BPR-1368"
title: "GitHub's REST API refuses tour stop edits on a pending review (404)"
status: "Ready"
rfcs: []
depends_on: ["BPR-1367"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","found_by":"BPR-1367 live probe, MikeMarcin/BPR-Test#45 (2026-10-09)","reservation":"docs/issues/_reservations/BPR-1368--BPR-1368--dad859faccd2.json","related_cards":["BPR-1367","BPR-1222","BPR-1223","BPR-1345"],"related_rfcs":["RFC-BPR-0041"]}
---

# BPR-1368: GitHub's REST API refuses tour stop edits on a pending review (404)

## Finding

Native tour publication ([RFC-BPR-0041](../../rfcs/RFC-BPR-0041-staged-tour-publication-and-recovery.md))
creates one pending review that holds every stop. It then edits each stop's
body to link the stops to one another, and only then submits the review with
COMMENT. `staged_operation::update_comment` edits a stop with
`PATCH /repos/{o}/{r}/pulls/comments/{id}`.

The [[BPR-1367]] probe on `MikeMarcin/BPR-Test#45` (2026-10-09, now closed)
found that, while the review is pending:

- `PATCH /pulls/comments/{id}` answers **404**, and so does
  `GET /pulls/comments/{id}`. After Submit the same PATCH answers 200.
- GraphQL `updatePullRequestReviewComment(pullRequestReviewCommentId, body)`
  edits the pending comment and returns its `line` and `startLine`.
- GraphQL `PullRequestReview.comments` lists the pending comments with `line`,
  `startLine` and `subjectType`. A comment has no side field; the side is on
  its review thread.

So live native publication stops at its second write. The first stop edit is
witnessed as possibly sent and then answered 404. Reconciliation finds the
body unchanged and reports the outcome unknown, and the tour cannot finish. The
fixtures accept a REST PATCH on a pending comment, so the tests do not show
this.

RFC-BPR-0041 already allows "a fixed GraphQL comment-body update ... if it is
the verified pending-edit route".

## Acceptance

- The fixture servers (`src/app/src/review_submission_fixture.cpp`,
  `tests/github_contract/staged_review_session_tests.cpp`) answer a REST
  `PATCH` or `GET /pulls/comments/{id}` on a pending review's comment with 404,
  as GitHub does. With them, the native tour tests fail before the fix.
- Pending stop edits use a verified route. The expected one is the fixed
  GraphQL `updatePullRequestReviewComment` mutation, added to the GraphQL
  allowlist for exactly this operation, with its own durable witness and
  reconciliation. Published-tour maintenance keeps using REST PATCH, which
  works after Submit.
- Optionally, confirm a pending stop's lines before Submit from GraphQL
  (`line`, `startLine`, and the side from the review thread), instead of only
  after Submit as BPR-1367 does.
- One live run on `MikeMarcin/BPR-Test` publishes a two-stop native tour end to
  end (create pending, edit both stops, submit) and checks the lines and
  bodies on GitHub. Close the pull request and delete its branch afterwards,
  and guard the run with `X-RateLimit-Remaining`. This is row evidence for
  [[BPR-1222]], not its qualification.

## Notes

The GHES route ([[BPR-1223]]) needs its own evidence. Do not assume that
GitHub.com's behaviour holds on GHES.
