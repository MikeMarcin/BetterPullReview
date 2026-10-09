---
schema: "implement-rfc.card.v1"
id: "BPR-1367"
title: "Tour publication reads a review's lines from a list that has none"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","found_by":"coordinator, from BPR-1345's live probe on MikeMarcin/BPR-Test#44 (2026-10-09)","reservation":"docs/issues/_reservations/BPR-1367--BPR-1367--94fb6975fe02.json","related_cards":["BPR-1345","BPR-1368","BPR-1222","BPR-1223"],"related_rfcs":["RFC-BPR-0041","RFC-BPR-0042"],"live_probe":"MikeMarcin/BPR-Test#45, opened and closed 2026-10-09, branch bpr-1367-probe deleted"}
acceptance: [{"criterion":"BPR-1367:bug-confirmed","outcome":"pass","revision":"dbfc82a66","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1367-tour-reads-submitted-review-lines-from-the-comment-collection.md","detail":"Live on MikeMarcin/BPR-Test#45: GET /pulls/45/reviews/{id}/comments returned no line, side, start_line, start_side or subject_type, both pending and submitted. staged_comment_entity required line and side, so parse_staged_review_response refused GitHub's real answer (identity_mismatch). With fixtures in that shape, main's code fails staged_review_session ('exact batch and optional description finish') and application_review_tour_publication_{native,stop,description,maintenance,reconciliation,manual}."}},{"criterion":"BPR-1367:collection","outcome":"pass","revision":"dbfc82a66","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1367-tour-reads-submitted-review-lines-from-the-comment-collection.md","detail":"A submitted tour review's comments come from the pull request's comment collection, through BPR-1345's parse_review_comment_collection, now shared and paged (observe_comment_collection). staged_review_session: WrongLine reports a stop a line off after Submit and the result is conflict, not published."}},{"criterion":"BPR-1367:pagination","outcome":"pass","revision":"dbfc82a66","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1367-tour-reads-submitted-review-lines-from-the-comment-collection.md","detail":"Every page up to maximum_review_comment_collection_pages (10, 1,000 comments); a next link past it, to another page or another origin is refused. github_mutation_contract test_review_comment_collection_pages; staged_review_session CollectionNoise: 100 other comments (outdated, file, cross-side, reply) on page 1, the tour's on page 2, published."}},{"criterion":"BPR-1367:fixture-shape","outcome":"pass","revision":"dbfc82a66","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1367-tour-reads-submitted-review-lines-from-the-comment-collection.md","detail":"review_submission_fixture.cpp and the staged_review_session test server drop line, side, start_line, start_side and subject_type from /reviews/{id}/comments, and serve /pulls/{n}/comments?per_page=100&page=N with only submitted reviews' comments. Lines are never read from the per-review list even when present (github_mutation_contract)."}},{"criterion":"BPR-1367:regressions","outcome":"pass","revision":"dbfc82a66","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1367-tour-reads-submitted-review-lines-from-the-comment-collection.md","detail":"ctest --preset dev (mac): staged_review_session, staged_review_restart, github_mutation_contract, live_review_submission_provider, live_review_submission_restart_recovery, live_review_mutation_provider, live_review_mutation_restart_recovery, gh_cli_transport, application_start_review_contract, application_review_round_trip_composition_contract, application_review_tour_publication_* (9): 19/19 passed."}}]
---

# BPR-1367: Tour publication reads a review's lines from a list that has none

## Finding

[[BPR-1345]]'s live probe on `MikeMarcin/BPR-Test#44` found that GitHub's list
of one review's comments, `GET /repos/{o}/{r}/pulls/{n}/reviews/{id}/comments`,
has no `line` or `side`. BPR-1345 moved its submission check to the pull
request's comment collection, `GET /repos/{o}/{r}/pulls/{n}/comments`. The
coordinator asked whether the review tour's staged publication
([RFC-BPR-0041](../../rfcs/RFC-BPR-0041-staged-tour-publication-and-recovery.md))
reads the same list and expects lines.

It does. `staged_operation::list_comments` reads `/reviews/{id}/comments`, and
`staged_comment_entity` refused any comment without `line` and a `LEFT` or
`RIGHT` side. GitHub's real answer therefore failed every read of a tour
review's comments, whether binding identities after the pending review is
created, verifying before each write, reconciling after Submit, or verifying a
published tour before maintenance. The fixtures served `line` and `side` on
that list, so every test passed on a shape GitHub never sends. Tour operations
stay unavailable in the product until [[BPR-1222]] and [[BPR-1223]] qualify
them, so no user reached this path.

## Live probe (MikeMarcin/BPR-Test#45, 2026-10-09)

One pull request on the designated target: branch `bpr-1367-probe`, a six-line
file, and one review with a single-line and a range comment, both `RIGHT`. The
pull request was closed and the branch deleted afterwards. The probe made 15
REST calls and 3 GraphQL calls (one GraphQL query was refused for asking a
comment's `diffSide`, which it does not have). `X-RateLimit-Remaining` stayed
between 4978 and 4967 throughout.

| Read or write | Pending review | Submitted review |
|---|---|---|
| `GET .../reviews/{id}/comments` | 200; keys have no `line`, `side`, `start_line`, `start_side`, `subject_type` (only `position`, `original_position`) | the same |
| `GET .../pulls/{n}/comments` | 200, **no comments**: a pending review's comments are not listed, not even to their author | 200, `line`, `side`, `start_line`, `start_side`, `subject_type` all present |
| `GET /pulls/comments/{id}` | **404** | (not tried) |
| `PATCH /pulls/comments/{id}` | **404** | 200, full fields |
| GraphQL `PullRequestReview.comments` | `line`, `startLine`, `subjectType` (no side field on a comment) | (not tried) |
| GraphQL `updatePullRequestReviewComment` | edits the pending comment | (not tried) |

The two 404s mean the tour's pending phase cannot edit its stops through REST at
all. The tour patches every stop's body while its review is pending, to link
the stops to one another, so live native publication fails at its second write
even with this fix. That is filed as [[BPR-1368]].

## Fix

- **Shared collection read.** BPR-1345's `parse_review_comment_collection`
  becomes the one reader of the pull request's comment collection, used by the
  submission check and the tour alike. It now reads a given page, requires a
  `Link` next link to name the following page of the same list on the expected
  API origin, and carries each comment's node ID, web and pull request URLs
  and author node ID. `review_submission_session::observe_comment_collection`
  reads every page up to `maximum_review_comment_collection_pages` (10, so
  1,000 comments). A longer collection is refused, never read in part: the
  tour then reports an unknown outcome or a conflict, and the submission is
  not confirmed. The request names its page
  (`/comments?per_page=100&page=N`), and outbound validation recompiles it for
  that page. Other reviewers' comments in it are structure only. Outdated
  comments (lines null, sides kept), file comments, ranges across sides and
  replies are well formed and no longer refuse the whole collection.
  BPR-1345's match also checks a range's start side.
- **Submitted tour reviews** (`observe_staged_comments`, chosen by the
  review's observed state) are read from the collection and converted by
  `staged_review_comments`, which holds each to exactly what
  `staged_comment_entity` holds a tour comment to: mine, on the expected head,
  on a line, not a reply, its own pull request URL and `#discussion_r` anchor.
  Lines are compared exactly. Published-tour maintenance reads the collection
  once for all the reviews it checks.
- **Pending tour reviews** can only be read through their own list, which
  has no lines. Those comments are parsed without a source: lines and sides it
  might carry are never read, and the path must be present. They are matched
  by ID, node ID, URL, author, review, head and body. After the pending review
  is created each takes the line it was created at, and each stop's initial
  body is distinct (it carries the stop number). Their lines are confirmed from
  the collection when the review is submitted. A stop reported anywhere else is
  then a conflict.

## Acceptance

- Bug confirmed against GitHub's real per-review shape: live probe above, and
  main's code fails with fixtures in that shape. **Pass.**
- The tour's confirmation reads lines from the pull request's collection, with
  the helper shared with BPR-1345. **Pass.**
- Pagination: every page up to an explicit bound of 10. **Pass.**
- The fixture servers' per-review endpoint drops `line` and `side` (and
  `start_line`, `start_side`, `subject_type`), as GitHub does. **Pass.**
- A test fails before and passes after. **Pass.**

## Verification (mac, dev preset, worktree `bpr-1367`)

Before the fix (`main`'s five production files with this card's fixtures and
tests): `staged_review_session` failed "exact batch and optional description
finish", and six of nine `application_review_tour_publication_*` courses
failed: native, stop, description, maintenance, reconciliation and manual. The
compat, conflict and unsupported courses read no review comments.

After the fix, at `dbfc82a66`: `staged_review_session`, `staged_review_restart`,
`github_mutation_contract`, `live_review_submission_provider`,
`live_review_submission_restart_recovery`, `live_review_mutation_provider`,
`live_review_mutation_restart_recovery`, `gh_cli_transport`,
`application_start_review_contract`,
`application_review_round_trip_composition_contract` and the nine
`application_review_tour_publication_*` courses all passed: 19 of 19, twice
in parallel and once more after the last rename. `application_start_review_contract`
failed once, in the first parallel run (40 s against its usual 9 s), with
`PUBLIC_PATH_FAILED`. It passed in five runs since, three of them serial, and
the failure did not reproduce, so its cause is not known. The round-trip
source pin is unchanged. Touched C++ was formatted with clang-format 22.1.8.
clang-tidy on the touched translation units reports nothing on a changed line;
the findings it does report are older ones on unchanged lines.

New tests: `github_mutation_contract` `test_review_comment_collection_pages`
and the per-review/collection cases in `test_staged_response_contract`;
`staged_review_session` CollectionNoise and WrongLine.

## Not covered

- Live native tour publication still fails, at the first pending stop edit
  (REST PATCH 404). See [[BPR-1368]].
- A pending stop's line is confirmed only after Submit, because no REST
  listing reports it before then.
- `review_mutation_session::repair_pending_publications` (single comments) reads
  the collection's first page with the new paged request and still refuses a
  `Link`, as before.

## Windows build fix (bpr-main-win, 2026-10-09)

`src/app/src/review_submission_fixture.cpp` did not compile with MSVC. The
paged comments listing took `page = std::stoul(...)` and then called
`std::min(listed.size(), (page - 1U) * 100U)`. On Windows `unsigned long` is
32 bits, so `std::min` was given `size_t` and `unsigned long` (C2672, C2737).
Linux and macOS are LP64, so they compiled. The page is now
`static_cast<std::size_t>(std::stoul(...))`. The composition library builds
on Windows (MSVC 19.50, Debug), and the round-trip pins are unchanged
(`PIN_CHECK_OK`).
