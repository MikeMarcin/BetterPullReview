---
schema: "implement-rfc.card.v1"
id: "BPR-1540"
title: "Name why a review submission stays unknown, with the attempt and in the diagnostics log"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-1538"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner approval 2026-10-15 of BPR-1538's follow-up (relayed by the coordinator)","governing_rfc":"RFC-BPR-0008","related_cards":["BPR-1538","BPR-1345","BPR-1367","BPR-0614","BPR-0971"],"reservation":"docs/issues/_reservations/BPR-1540--BPR-1540--da4bf96619cc.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8fed598f1","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1540-name-why-a-review-submission-stays-unknown.md","detail":"core_review_submission test_unknown_reasons: 20 stable names, exact describe/parse round trip, invalid diagnoses never written, nine near-miss lines refused."}},{"criterion":"AC2","outcome":"pass","revision":"8fed598f1","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1540-name-why-a-review-submission-stays-unknown.md","detail":"live_review_submission_provider unknown_reasons_are_named_kept_and_logged: 20 causes x GitHub.com and GHES 3.18 each name their reason with counts through the production session; each control without its cause publishes and names, keeps and logs nothing; mutation controls (wrong reason, no log, no store) fail it."}},{"criterion":"AC3","outcome":"pass","revision":"8fed598f1","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1540-name-why-a-review-submission-stays-unknown.md","detail":"sqlite_review_submission_recovery_storage test_unknown_reason_is_kept_with_the_attempt: kept across restart, replaced, carried by pending, corrupt value fails closed, cleared on resolution, gone with withdrawal, v5 migrates to v6; migration rows updated to 6 with 7 refused."}},{"criterion":"AC4","outcome":"pass","revision":"8fed598f1","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1540-name-why-a-review-submission-stays-unknown.md","detail":"Row checks: detail only [a-z0-9_= ] with no host, repository, account, path or comment canaries. quill_logging: real file keeps op, domain, name and exact line; mismatched, invalid or misplaced reasons dropped. application_release_maintenance test_review_submission_reason_export: exact line kept, PRIVATE_ canaries omitted or dropped under both policies."}},{"criterion":"AC5","outcome":"pass","revision":"8fed598f1","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1540-name-why-a-review-submission-stays-unknown.md","detail":"Every case asserts one POST; unknown outcomes stay indeterminate; live_review_submission_restart_recovery and composed application_start_review_contract and application_review_submission_composition_public_path pass."}},{"criterion":"AC6","outcome":"pass","revision":"8fed598f1","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1540-name-why-a-review-submission-stays-unknown.md","detail":"22 related rows and four architecture boundary rows pass on macOS; clang-format 22.1.8 check passes; clang-tidy clean on changed lines; verify_documentation DOCUMENTATION_OK; non-testing composition syntax-checked."}}]
---

# BPR-1540: Name why a review submission stays unknown, with the attempt and in the diagnostics log

[[BPR-1538]] fixed the reads that left a landed review with comments unknown
on every Check status. Its owner's case was on a protected GitHub Enterprise
Server, and nothing the application wrote said why: an indeterminate result
carried no reason, the attempt's record kept none and the diagnostics log had
no event. The owner approved the follow-up on 2026-10-15: when Submit or
Check status leaves a review unknown, record a reason code.

Every value recorded is an enum or a count. No host, repository, URL, account
name, path, token or comment words are recorded, because the owner uses a
protected GHES and the log can be exported.

## Design

- **Core decides the vocabulary.** `bpr/core/review_submission_unknown.hpp`
  defines the closed `review_submission_unknown_reason` (20 values), the POST
  answer (`not_sent`, `success`, `error_status`, `no_answer`) and
  `review_submission_unknown_diagnosis`. That is one reason, the POST and read
  status classes (2 to 5), and counts: comments carried, comments the new
  review holds, comments matched, new reviews of mine, comment pages read.
  Core also gives the stable names, a validity rule, `describe` (one line of
  fixed keys and decimals) and `parse_review_submission_unknown`, which reads
  back exactly what `describe` writes and nothing else.
- **The host fills it.** `review_submission_session` keeps the first reason a
  run could not continue, from typed facts only: the transport's provider
  failure, the HTTP status class, the contract's parse failure and its own
  match counts. Every indeterminate result carries `Unknown`; other results
  carry none. The comment-collection parser now tells a link that is not this
  list's (`unexpected_headers`) and a list longer than its pages
  (`collection_limit`) apart from a body that is not GitHub's list, and reads
  a link's page number rather than matching it against the bound.
- **Kept with the attempt.** The recovery database is now schema 6.
  `submission_unknown` is a child of `submission_attempt` with checked integer
  columns only. `note_unknown` replaces the reason; `resolve` deletes it; a
  withdrawn attempt takes it with it; `load` and `pending` return it as
  `LastUnknown`. A v5 database migrates in place. As before, a newer schema
  is refused by an older build.
- **Written to the diagnostics log.** One `github.review_submission.failed`
  event per unknown result. `failure_domain` is `review_submission`, `failure`
  is the reason name and `detail` is the `describe` line. The Quill sink drops
  a reason on any other operation, an invalid diagnosis, or a detail that is
  not exactly its line. The release export admits the operation, domain and
  names and keeps the detail only when it parses back exactly.
- **The compose page is unchanged.** The page may show a short reason under
  "Review status is unknown", but this card does not add one. Visible text
  needs a look-and-judge question, and this change is complete without it.
  Presenting the reason is left for a separate decision.

| Reason | Recorded when |
|---|---|
| `review_not_listed` | no new review of mine at the original head with this summary and verdict |
| `several_reviews_listed` | more than one such review |
| `review_list_unreadable` / `_too_large` / `_paged` | the review list is not GitHub's list, over 3 MiB, or continues past 100 |
| `comment_page_unreadable` / `_too_large` / `_link_refused` | a comment page is not GitHub's list, over 3 MiB, or names a next page that is not this list's |
| `comment_collection_too_long` | the comments continue past 10 pages |
| `comments_mismatch` | the new review does not hold exactly these comments (counts held and matched) |
| `comment_moved_without_original_position` | a comment moved off the head and the server names no original position |
| `http_status` / `rate_limited` / `timeout` / `network` | a read was answered with another status (class kept), rate limited, timed out or failed in transit |
| `account_mismatch` / `pull_request_mismatch` | the account or pull request read back is not the one submitted to |
| `connection_unavailable` | no connection, credential or permission now |
| `local_record_unavailable` | the attempt's local record is missing or unreadable |
| `cancelled` | the read was stopped |

## Acceptance

- **AC1, vocabulary:** core names every reason once, writes and reads back
  the counts-only line exactly, and refuses anything else.
- **AC2, every reason reached:** for GitHub.com and GHES targets, each of the
  20 reasons is produced by its cause through the production session, with
  its counts. Without the cause, the same arrangement is published and
  nothing is named, kept or logged.
- **AC3, kept:** the reason survives restart with the attempt, is replaced by
  the next, is cleared on resolution and withdrawal, and fails closed when
  corrupt; v5 migrates.
- **AC4, logged and exportable without private content:** one counts-only
  event per unknown result; its words are only enum names, keys and digits;
  hostile or mismatched details are dropped or omitted on export.
- **AC5, unchanged safety:** no second POST in any case; an unknown outcome
  stays unknown.
- **AC6, gates:** focused rows, clang-format 22, tidy, documentation gate.

## Evidence

All on macOS, Debug without debug information (to save disk space), through
the network-denied test runner.

- `core_review_submission` `test_unknown_reasons`: each of the 20 reasons has
  one name and round-trips through `describe` and
  `parse_review_submission_unknown`. Out-of-range classes, answers and counts
  are not valid and are never written. Nine near-miss lines are refused:
  trailing space, a prefix, an extra `host=` key, a missing key, an unknown
  name, a leading zero, a count over 65,535 and keys out of order.
- `live_review_submission_provider`
  `unknown_reasons_are_named_kept_and_logged`: 20 cases × GitHub.com and GHES
  3.18 × with and without the cause, on full-size GitHub-shaped pages
  ([[BPR-1538]]'s server).
  - With the cause, the result is indeterminate and names exactly that
    reason, with the review's three comments counted and one POST. The
    attempt keeps the same diagnosis, except `local_record_unavailable`,
    which has no attempt. The log receives one `review_submission` event,
    outcome failed, request 0, HTTP 0. Its detail equals `describe`, contains
    only `[a-z0-9_= ]`, and none of the host, repository, account, path or
    comment-word canaries.
  - Without the cause, the review is published, nothing is named or logged,
    and the attempt keeps no reason.
  - All 20 reasons are reached. A mismatch counts 3 held, 2 matched,
    1 candidate, 2 pages and the POST's 5xx answer.
- Mutation controls on that row: naming `comment_page_unreadable` for a
  refused link, skipping the log, or skipping the store each fail it at the
  named step.
- `live_review_submission_restart_recovery`: a witnessed attempt's second
  recovery run keeps its record exactly, plus the `review_not_listed` reason
  the first run recorded.
- `sqlite_review_submission_recovery_storage`
  `test_unknown_reason_is_kept_with_the_attempt`:
  - No reason is kept for an unknown key, another fingerprint or an invalid
    diagnosis.
  - A reason is kept, survives restart, is replaced by the next and is
    carried by `pending`.
  - An out-of-set reason column fails closed.
  - Resolution clears the reason and refuses another; withdrawal leaves
    nothing behind.
  - v5 migrates to v6.
  - `sqlite_review_mutation_recovery_storage` migrations now land on 6, and
    7 is the refused future version.
- `quill_logging`:
  - A reason reaches the real file as `"op":"github.review_submission"`,
    `failure_domain` `review_submission`, its `failure` name and its exact
    detail line.
  - A changed detail, an invalid diagnosis or another operation is dropped.
  - The metadata-only preview classifies the record (`operation=6`) without
    copying its detail.
- `application_release_maintenance` `test_review_submission_reason_export`,
  both export policies: the exact line is kept; a line with an extra
  `host=PRIVATE_HOST` or a `PRIVATE_REPOSITORY` reason is omitted; a
  `PRIVATE_CANARY` failure name drops the record; no `PRIVATE_` text is
  exported.
- Regressions: 22/22 related rows pass, including `gh_cli_transport`,
  `github_mutation_contract`, `staged_review_session`/`_restart`,
  `live_review_mutation_provider`/`_restart_recovery`,
  `sqlite_private_store_open`, `application_start_review_contract` and
  `application_review_submission_composition_public_path`. These run the
  production session through the composed application.
  `core_dependency_boundary`, `core_boundary_contract`,
  `architecture_boundaries` and `architecture_boundary_contract` pass.
- The non-testing composition branch (live provider with the log) passes a
  `-fsyntax-only` compile of `github_application.cpp` without the testing
  defines.
- clang-format 22.1.8 check of the 23 touched C++ files passes.
- clang-tidy with warnings as errors reports no finding in changed lines. The
  pre-existing findings in `review_mutation_recovery_store.cpp` (codec lambda
  names, staged optional) and `review_tour_codec.hpp`, and in the test helpers
  `diagnostic_json.hpp`, `quill_logging_tests.cpp` line 97 and
  `review_submission_recovery_storage_tests.cpp` line 315, are on `main`
  unchanged.
- `python3 scripts/verify_documentation.py --root .` passes.

Not done:
- Visible text is unchanged, so there is no attended question.
- Windows and Linux runs of these rows are owed by the stations.
- A build with the old schema 5 refuses a v6 recovery database, as each
  earlier schema bump did.
