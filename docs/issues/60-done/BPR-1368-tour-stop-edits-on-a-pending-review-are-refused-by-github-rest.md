---
schema: "implement-rfc.card.v1"
id: "BPR-1368"
title: "GitHub's REST API refuses tour stop edits on a pending review (404)"
status: "Done"
rfcs: []
depends_on: ["BPR-1367"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","found_by":"BPR-1367 live probe, MikeMarcin/BPR-Test#45 (2026-10-09)","reservation":"docs/issues/_reservations/BPR-1368--BPR-1368--dad859faccd2.json","related_cards":["BPR-1367","BPR-1222","BPR-1223","BPR-1345"],"related_rfcs":["RFC-BPR-0041"],"live_probe":"MikeMarcin/BPR-Test#46, opened and closed 2026-10-09, branches bpr-1368-live-base and bpr-1368-live-head deleted"}
acceptance: [{"criterion":"BPR-1368:fixtures-404","outcome":"pass","revision":"2ad076bcb","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1368-tour-stop-edits-on-a-pending-review-are-refused-by-github-rest.md","detail":"review_submission_fixture.cpp and the staged_review_session test server answer REST PATCH or GET /pulls/comments/{id} on a pending review's comment with 404 and serve the GraphQL edit. With main's production code (fa9934adc) and these fixtures, staged_review_session ('exact batch and optional description finish'), staged_review_restart and application_review_tour_publication_{native,stop,description,maintenance,reconciliation,manual} fail: 8 of 11."}},{"criterion":"BPR-1368:graphql-route","outcome":"pass","revision":"2ad076bcb","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1368-tour-stop-edits-on-a-pending-review-are-refused-by-github-rest.md","detail":"staged_review_operation::update_pending_comment compiles to the fixed GraphQL mutation UpdateReviewComment (updatePullRequestReviewComment by node ID), a witnessed write reconciled from the review's own comment list; its answer must name the REST comment, review and pull request, PENDING. Used for the initial tour and a maintenance plan's added stops; submitted stops keep REST PATCH. gh api graphql admits only it. github_mutation_contract test_a_pending_stop_is_edited_by_graphql; gh_cli_transport staged catalog row; staged_review_session asserts two GraphQL edits and no REST pending edit."}},{"criterion":"BPR-1368:regressions","outcome":"pass","revision":"2ad076bcb","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1368-tour-stop-edits-on-a-pending-review-are-refused-by-github-rest.md","detail":"ctest --preset dev (mac): staged_review_session, staged_review_restart, application_review_tour_publication_* (9), github_mutation_contract, gh_cli_transport, github_read_contract, live_review_{mutation,submission}_provider, live_review_{mutation,submission}_restart_recovery, sqlite_review_{mutation,submission}_recovery_storage, native_https_transport_{contract,fixture}, application_review_round_trip_composition_contract, application_start_review_contract: 24/24. Tour CLI tree (dev + BPR_BUILD_TOUR_CLI=ON): tour_cli_publication, tour_cli_terminal_publication 2/2."}},{"criterion":"BPR-1368:live-tour","outcome":"pass","revision":"2ad076bcb","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1368-tour-stop-edits-on-a-pending-review-are-refused-by-github-rest.md","detail":"MikeMarcin/BPR-Test#46 (2026-10-09, closed, both branches deleted): staged_review_session --live-tour, the production session over the production gh api transport (gh 2.101.0, macOS), published a two-stop native tour: create pending, both stops edited by GraphQL while pending (GitHub's answers parsed by the production parser), COMMENT submit, lines confirmed from the collection. Independent gh api read: review COMMENTED with the final index; stop 1 RIGHT 9-11, stop 2 LEFT 5; both bodies the final linked bodies. Row evidence for BPR-1222, not its qualification."}}]
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

## Fix

- **One GraphQL write in the staged catalog.** `staged_review_operation::update_pending_comment`
  compiles to the fixed mutation `UpdateReviewComment`
  (`updatePullRequestReviewComment(input:{pullRequestReviewCommentId,body})`)
  at the host's GraphQL root, addressed by the comment's node ID. The request
  also carries the REST comment and review IDs, and the answer must name
  them (`fullDatabaseId` on the comment and its `pullRequestReview`), the
  designated pull request, the acting user, the body sent and `PENDING`. The
  operation is a witnessed write; reconciliation reads the review's own
  comment list, which GitHub serves while the review is pending, exactly as
  for the REST edit. `review_mutation_operation::staged_update_pending_comment`
  is classified POST, GraphQL, witnessed.
- **Which edit is which.** `staged_pending_edit` builds it for the record's
  next unpatched comment while its own review is pending: the initial
  publication's navigation edits and a maintenance plan's added stops, both
  before Submit. Edits of already submitted stops in maintenance stay
  REST `PATCH /pulls/comments/{id}`, which GitHub answers 200 after Submit.
  Compatibility issue comments are unchanged.
- **Durable request.** The staged record codec moves to version 4 to keep the
  pending request's node ID; versions 1 to 3 still read.
- **Transports.** Native HTTPS already routes GraphQL writes by the
  operation's exchange. `gh api graphql` admits exactly this operation and no
  other GraphQL write. The read contract reserves the name
  `UpdateReviewComment`, so the document is refused on the read transport.

## Verification (mac, dev preset, worktree `bpr1368`)

Before the fix: `fa9934adc`'s `src/adapters/github`, `src/adapters/sqlite` and
`src/application` with this card's two fixture servers. `staged_review_session`
failed "exact batch and optional description finish"; `staged_review_restart`
and `application_review_tour_publication_{native,stop,description,maintenance,reconciliation,manual}`
failed (native: "normal Publish reaches verified success"): 8 of the 11.

After, at `2ad076bcb`: the 24 rows named in the acceptance front matter pass,
and in a second tree configured `dev` with `BPR_BUILD_TOUR_CLI=ON`,
`tour_cli_publication` and `tour_cli_terminal_publication` pass. Touched C++
was formatted with clang-format 22.1.8; clang-tidy on the eight touched
production translation units reports nothing on a changed line. The round-trip
composition pins did not move.

New tests: `github_mutation_contract` `test_a_pending_stop_is_edited_by_graphql`
(exact document and variables, GHES root, tamper, identity refusals, answer
parse and six wrong answers, GraphQL error, non-200); the staged rows of the
exchange and request-shape tables; `gh_cli_transport`'s staged catalog row (only
this operation uses `gh api graphql`); `staged_review_session` asserts two
GraphQL edits and no refused REST edit.

## Live run (MikeMarcin/BPR-Test#46, 2026-10-09)

A disposable pull request on the designated target: branch
`bpr-1368-live-base` with a 12-line `bpr-1368/tour.txt`, and
`bpr-1368-live-head` removing base line 5 and adding head lines 9 to 11.
`staged_review_session --live-tour DIR` (hand-run, not a registered row) ran
the production `review_submission_session::publish_tour` over the production
`gh_cli_transport` with `gh` 2.101.0, a SQLite journal in a scratch folder and
a relay that logs each request and reads `X-RateLimit-Remaining`. The tour had
two stops: a RIGHT range on head lines 9 to 11 and a LEFT stop on base line 5.

- Published: create pending, the two stop edits by GraphQL while pending,
  COMMENT submit, and the lines read back from the pull request's comment
  collection. No REST edit of a pending comment was sent. GitHub's two GraphQL
  answers passed `parse_staged_review_response`, so the pending comment's
  `url` is the `#discussion_r{id}` link the tour holds.
- Checked independently with `gh api` afterwards: review `5472789413` is
  `COMMENTED` with the final index body; comment `4232362399` is RIGHT 9 to 11
  and `4232362409` LEFT 5; each body is its final body and links the other stop
  and the overview.
- The pull request was closed and both branches deleted.

Calls: 48 REST (7 setup, 36 by the session, 2 verify, 3 cleanup) and 4 GraphQL
(the session's 2 edits and 2 schema reads beforehand, one refused for
introspection depth). `X-RateLimit-Remaining` stayed at or above 4967 (REST)
and 4972 (GraphQL). The harness stops sending below 500.

This run used the session and `gh api` transport on macOS only. It is row
evidence for [[BPR-1222]], not its qualification.

## Not covered

- A pending stop's lines are still confirmed only after Submit. The optional
  GraphQL read of `line`, `startLine` and the review thread's side before
  Submit was not added.
- Native HTTPS was not run live; its fixture rows are unchanged (it routes the
  operation by the generic GraphQL exchange).
- GHES ([[BPR-1223]]) has no evidence for this route; the compiled request
  names `/api/graphql`, nothing more.
- A journal written before this change whose pending request is a REST edit
  of a pending stop no longer matches the next request the record derives, so
  it is refused as invalid (a conflict), never resent. Tour operations have
  not been available in production, so no such record is expected.
