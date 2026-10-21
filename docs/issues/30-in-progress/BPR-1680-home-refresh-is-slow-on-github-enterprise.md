---
schema: "implement-rfc.card.v1"
id: "BPR-1680"
title: "Home refresh is slow on GitHub Enterprise"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner report, 2026-10-20: Home takes a long time to refresh on a GHE environment; anonymized diagnostic export supplied out of band (not committed)","reservation":"docs/issues/_reservations/BPR-1680--BPR-1681--3313b46dcf00.json","related_cards":["BPR-1681","BPR-1679","BPR-0988","BPR-0989","BPR-0990","BPR-1370","BPR-1379","BPR-1436"],"owner_decision":"2026-10-21: options 1 to 5 approved, in order, one commit each; options 6 and 7 not approved"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"option 1 commit on 97c35a45b","evidence":{"kind":"command","detail":"core_home_discovery (line format, read-back, every refusal), application_review_queue_enrollment's new test_discovery_records_each_refresh_and_read (ten lines for one refresh of four reads: starts, verbose read starts, read ends with requests, lane wait and run time, a failed read's failure, the refresh's totals and duration; a session change ends a refresh cancelled), quill_logging (a discovery read's end reaches the real file with its failure; any other detail is dropped), application_release_maintenance's new test_discovery_export (both export policies keep exactly the counts-only line and omit one with a repository or login in it), and application_personal_discovery_public_path and its CLI variant reading the composed profile's own diagnostics file (every refresh with its reads and requests, read ends, no viewer login or repository name) pass. application_home_waiting_public_path and application_home_open_feedback_public_path pass."}},{"criterion":"AC2","outcome":"pass","revision":"option 2 commit over the option 1 commit","evidence":{"kind":"command","detail":"core_home_discovery's test_known_pull_requests (only open, fully read pull requests, ordered, once each; a requested row stays only when the viewer is asked by name), github_paged_read (known rows come back Reused with no requested-reviewer, pull request or summary read; known rows of another repository, out of order, invalid, or on a retained read are refused before any request; a retained read reports RequestsViewer in any case), application_review_queue_enrollment's new test_discovery_searches_reuse_the_refresh_retained_reads (both searches know the three fully read pull requests and not the bounded one; two rows reused; a team-only reused row is not requested; an authored reused row takes the retained detail; a search-only row is read as before; an unknown Reused row is dropped), and the four fixture courses pass. application_personal_discovery_public_path asserts three reuses on its explicit Refresh: 19 requests before, 13 after; its restart refresh 23 before, 17 after."}},{"criterion":"AC3","outcome":"pass","revision":"option 3 commit over the option 2 commit","evidence":{"kind":"command","detail":"core_home_discovery's test_search_plan (exact queries; five short repositories in one search; 25 long ones split, each search inside the 256-byte and ten-repository bounds, every repository once and in order; refused ones alone and after; two of the longest names never share; case-different names never share; split only for a shared search refused or past its page or detail bound), github_paged_read (the shared query names both repositories in order; the same number in two repositories is two attributed rows; each detail read under its own repository; an item of an unnamed repository refused; shared repositories out of order, repeated, invalid, more than ten, on a retained read, or a known pull request of an unnamed repository refused before any request; compile refuses a case-insensitive repeat, a query past the bound, a path with a further slash, and the field on another operation), application_review_queue_enrollment's new test_discovery_searches_several_repositories_at_once (two searches for three repositories; rows applied to their repositories; a complete shared answer withdraws; past its page and refused shared searches split ahead of the rest; only the repository refused alone is unreadable; the next refresh searches it alone and the others together), and the four fixture courses pass. application_personal_discovery_public_path: Save Repository 11 requests to 9, explicit Refresh 13 to 11, restart refresh 17 to 19 (the refused shared search asked again alone, once per session)."}},{"criterion":"AC4","outcome":"pass","revision":"option 4 commit over the option 3 commit","evidence":{"kind":"command","detail":"core_home_discovery's test_retained_batches (grouped by repository in first-appearance order, ascending, cut at ten, each once; a bound of one is the old shape; node and cost bounds), github_paged_read's test_retained_home_batch_read (two pull requests cost one request; title, draft, head, author, reviews, review requests, code owners, merge state, failed checks and an erroring status, and change facts with pending review comments excluded; more reviews than a page read the REST way while the other is not; an undefinedField answer reads the REST way and reports it, and a known-unsupported profile is not asked again; 401 fails the read with no REST read; a wrong alias sends all the REST way; batches out of order, of one, repeated, of eleven, with a mismatched Number or on a search are refused before any request; the wire names the numbers as document literals), application_review_queue_enrollment's new test_discovery_batches_retained_reads (three retained pull requests of one repository are one read logged as three; answered rows applied, the left-out one fails alone; an unasked row fails the answer), github_read_contract, github_anonymous_read_contract and native_https_anonymous_read_contract (the operation is compiled, named and refused anonymously), and the four fixture courses plus the home-waiting CLI course, application_profile_multi_window_public_path, application_profile_two_account_isolation_public_path, application_profile_window_review_restoration_public_path, application_offline_inventory, application_remembered_connection_public_path and application_rate_limit_render_recovery pass. application_personal_discovery_public_path asserts one batched read with one pull request paged to REST: explicit Refresh 11 requests to 6, restart refresh 19 to 14."}}]
---

# Home refresh is slow on GitHub Enterprise

The owner reports that Home takes a long time to refresh against their GitHub
Enterprise server. This card records what the diagnostic export shows, what the
code does per refresh, and the options. No speed-up shipped with it: each
option below changes a design decision. The related defect, an export that
re-ran a stale refresh, is [[BPR-1681]].

## What the export shows

The export is anonymized, covers five retained sessions (builds 0.1.4 to 0.3.0)
and holds 316 records; the exporter's allow-list omitted 2,319 more. Every
response was HTTP 200: there is no primary or secondary rate limit, no 403 or
429, no retry and no failed GitHub read.

- **Home discovery is not in it.** Personal discovery records no diagnostic
  event; the log's operations are application, viewer, capability, pull
  request, epoch, review comment and review submission. The export therefore
  has no request count, timing, failure or retry for any Home refresh. The
  newest session lasted 42 seconds and recorded one failed viewer read and the
  four stages of gh CLI account discovery.
- **Requests run strictly one at a time.** In each of the nine pull request
  loads the responses arrive in sequence and never overlap. Every provider
  job, a pull request load or one discovery read, runs on the profile's one
  `ProviderEffects` worker, and on a gh CLI connection every request is also
  one process on the runner's single worker (`serialized_process_lane`,
  [[BPR-1436]]). The two discovery reads `personal_discovery_host` keeps in
  flight therefore run one after the other, and a pull request load queues
  behind a whole discovery read (one search with its rows' detail reads).
- **One request costs about 0.65 s on this server.** Medians per page: files
  0.56 s, commits 0.58 s, issue comments 0.54 s, reviews 0.73 s, checks 0.78 s,
  review threads 0.81 s, pull request 0.90 s; 90th percentile of plain REST
  pages 1.2 s. Search 1.8 s (1.4 to 4.5 s); timeline 3.7 s (0.8 to 13 s). Most
  of a plain page is the `gh` process and the round trip, not GitHub's work.
- **The capability probe** at session start is nine sequential requests:
  5.6 s, 8.7 s and 14.7 s in three sessions.
- **Waiting in the lane.** The first response of a pull request load came
  0.7 to 1.1 s after it started in four loads, and 7.4, 7.6, 16.6, 21.5 and
  66.0 s after it in the other five. The page timings after that first
  response are ordinary, so the extra seconds are spent queued behind other
  `gh` work (startup probes, Home discovery). [[BPR-1679]] now lets a pull
  request read go ahead of Home's discovery; that does not make the refresh
  itself shorter.

## What one refresh asks for

`personal_discovery_host` runs, per refresh (one `gh` process each):

- **Each retained pull request** (up to 30): `GET pull`, one or more review
  pages, then the Home summary ([[BPR-1379]]): merge assessment, review
  requests, check runs and commit statuses. About six requests.
- **Each saved repository**: two searches (`author:` and
  `review-requested:`), one search per request.
- **Each pull request a search finds** (up to 30 per search): requested
  reviewers, `GET pull`, then the same Home summary. About six requests.
- **A pull request that is both retained and found** is read twice in the
  same refresh: once as retained metadata and again as a search row.
- **On a gh CLI connection**, `gh_cli_transport::begin_operation` acquires a
  credential with its own `gh` process for every discovery read, and the
  observed account lease ([[BPR-1370]]) reads the viewer before and after each
  request; the public-path course asserts four viewer reads for two searches.

For example, 5 saved repositories, 20 retained pull requests and 15 found
rows is roughly 120 + 10 + 90 = 220 requests, about 2.5 minutes at 0.65 s
each, and two to three times that through the CLI lease. Each search on this
server costs about three times a page. This is an estimate from the code and
the timings above, not a measurement of the owner's Home.

## Options (owner decision)

1. **Record discovery in the diagnostic log.** One started/response/finished
   event per discovery read, with its reason and page operation, so the next
   export shows the refresh's count and duration. Small, but it adds
   diagnostic operations and allow-list entries; do it first, whichever else
   is chosen.
2. **Read a pull request once per refresh.** Skip the search row's detail and
   summary reads for a pull request whose retained read already ran in the
   same refresh, or the reverse. This changes what a search answer may claim
   (`Detailed`, requested-reviewer evidence for team-only requests).
3. **One search for all saved repositories.** `author:<login>` and
   `review-requested:<login>` with several `repo:` qualifiers (two searches,
   or a few for many repositories) instead of two per repository. Changes the
   per-repository completeness and forgetting semantics.
4. **One GraphQL query per batch of pull requests.** Title, head, reviews,
   requested reviewers and check rollup for several pull requests by alias,
   replacing about six REST reads each. Needs GHE schema coverage, the
   capability probe and query cost.
5. **Run more than one request at a time.** A second provider worker and a
   second `gh` worker, or a discovery lane beside the review lane. This
   changes [[BPR-1436]]'s serialized lane and touches the priority work in
   progress.
6. **Check the CLI account once per operation.** Read the viewer at the start
   and end of a discovery read rather than around each request. This weakens
   [[BPR-1370]]'s per-request account check and is a security decision.
7. **Refresh only what changed.** Conditional requests (ETag) where the server
   honours them, or a longer freshness window on slow servers. Smaller wins.

## Owner decision

2026-10-21, through the coordinator: options 1 to 5 are approved and are
done in that order, one commit each, each verified before the next. Options
6 (a once-per-operation CLI account check) and 7 (conditional requests or a
longer freshness window) are not approved.

## Acceptance

- AC1 (option 1): each Home refresh and each of its reads is in the
  diagnostics log with its reason, repository count, requests, lane wait, run
  time and how the refresh ended, as counts and enums only, and the anonymized
  export keeps exactly that line.
- AC2 (option 2): a pull request whose retained read ran in a refresh is not
  read again by that refresh's searches, and Home lists the same rows.
- AC3 (option 3): one search per reason covers several saved repositories,
  split when the query would be too long, with every row attributed to its
  repository and each repository's completeness kept.
- AC4 (option 4): retained pull requests are read in GraphQL batches within
  node and cost bounds, with a fallback where the server lacks a field and
  correct paging for large pull requests.
- AC5 (option 5): more than one request runs at a time, bounded, with a
  window's pull request read still first and cancellation, quarantine,
  process-group kill, the account-change refusal and the serialized completion
  lane intact.
- AC6: before and after request counts and timings from the fixture courses;
  the focused suites, format, tidy and documentation checks pass.

## Progress

**Option 1, done.** `github.discovery` events ([[BPR-1680]]):
`core::discovery_diagnosis` is one line of fixed keys, enums and decimal
counts (`discovery=SCOPE refresh=N read=N reason=REASON state=STATE
repositories=N pull_requests=N reads=N failed=N requests=N wait_ms=N
run_ms=N`), with `describe` and an exact `parse_discovery_diagnosis`. The
provider measures each read's `personal_discovery_cost` (wire requests through
a counting transport, wait for the provider lane, run time) and returns it
with the completion; `personal_discovery_host` records a refresh's start and
end (current, partial, stale, stopped or cancelled, with its reads, failures,
summed requests and lane waits, and its duration from the clock port's new
monotonic `elapsed_milliseconds`), each read's start (verbose) and each read's
end, with a failed read's failure. Refreshes and reads are numbered, not
identified, so nothing occupies the logger's identity table and nothing names
a host, repository, account or pull request. The logger drops a discovery
event with any other detail; the export's detail allow-list admits the line
only when it parses back; Insights counts a failed discovery read as an inbox
problem. On a CLI connection the lease's viewer reads around each request are
not counted in `requests`.

Baseline from the fixture courses, read from the composed profile's own
diagnostics file (refresh numbers are the course's):

| Course refresh | Saved repositories | Retained | Reads | Requests |
|---|---|---|---|---|
| Save Repository (refresh 2) | 2 | 0 | 4 | 11 |
| explicit Refresh (refresh 3) | 2 | 4 | 8 | 19 |
| restart, three refused (restart refresh 1) | 4 | 4 | 12 | 23 |

The fixture answers Home's summary reads only for its waiting-detail rows, so
a fixture retained read costs two requests rather than the six it costs on a
real server; durations on the fixture network are tens of milliseconds and are
not a timing measurement.

**Option 2, done.** A search carries `Known`, the pull requests of its
repository this refresh's retained reads read in full
(`core::discovery_known_pull_requests`: open, summary not bounded). The
adapter returns a found row for one of them `Reused`, without its
requested-reviewer, pull request and summary reads and without spending a
detail read; the host fills it from the retained read, keeps a requested row
only when that read named the viewer as a user (`core::keeps_reused_row`, the
retained read now reports `RequestsViewer` from the same `requested_reviewers`
array), and drops a `Reused` row the request did not know. A pull request the
retained read found closed or only partly read is searched and read as
before.

| Course refresh | Requests before | After option 2 |
|---|---|---|
| Save Repository (no retained) | 11 | 11 |
| explicit Refresh (4 retained, 3 found again) | 19 | 13 |
| restart, three refused | 23 | 17 |

The three reused rows save two fixture requests each; on a real server each
saves about six (requested reviewers, the pull request, review requests, merge
assessment, check runs, commit statuses). The course now asserts the three
reuses and its detail-read count; Home lists the same rows.

**Option 3, done.** `core::plan_discovery_searches` packs the saved
repositories, for each reason, into searches of at most ten repositories and
a whole query of at most 256 bytes (qualifiers counted, the conservative
reading of GitHub's limit), never two names of one repository in different
case. The request names them all (`Repositories`), the wire query is
`is:pr is:open author:LOGIN repo:A repo:B ...`
(`semantic_read_request::SearchRepositories`, validated on compile), the
adapter attributes each item by its `repository_url`, reads its detail under
its own repository and refuses an item of any other, and the host applies one
answer per named, still-saved repository. Per-repository completeness and
readability are kept by `core::splits_discovery_search`: a shared search GitHub
refuses (a 422 names no repository) or whose page or detail bound left rows
out is asked again one repository at a time, ahead of the rest, and a
repository then refused alone is searched alone for the rest of the session.
GitHub's own incomplete search and other failures apply to each named
repository, as they would have alone. `personal_discovery_options` bounds the
repositories per search; the host's older scheduler tests set it to one, the
shape they were written for, and the new tests and the composition use the
production bound.

| Course refresh | Before | Option 2 | Option 3 |
|---|---|---|---|
| Save Repository (2 repositories, 0 retained): reads, requests | 4, 11 | 4, 11 | 2, 9 |
| explicit Refresh (2 repositories, 4 retained) | 8, 19 | 8, 13 | 6, 11 |
| restart, 4 repositories, 3 refused | 12, 23 | 12, 17 | 14, 19 |

The restart refresh costs two more searches than option 2: the shared search
is refused because three of its repositories are, and is then asked one
repository at a time; later refreshes in the session search the refused three
alone, so this is paid once. On the owner's example of five saved
repositories, the ten searches of a refresh become two.

**Option 4, done.** The host groups a refresh's retained pull requests by
repository into reads of at most ten (`core::plan_retained_batches`); such a
read carries `Numbers`, and the adapter asks one GraphQL query,
`ListHomePullRequests`, with an alias per pull request and a
`HomePullRequest` fragment for the title, draft, state, head and base,
author, comment total, the first 50 reviews (each with its comment total),
review requests and last-commit check contexts, and the merge state, review
decision and auto-merge. It builds the same row the REST reads build: reviews
as submitted-review evidence, `ReviewersAwaited` and `RequestsViewer` from the
review requests, the waiting summary through the same snapshot fields and
`queue_waiting_for` (code owners, merge state, failed check runs and erroring
statuses, one per context), and change facts with review comments summed over
non-pending reviews. Bounds (`core`): ten pull requests and a page of 50,
about 1,500 nodes against GitHub's 500,000 and about six points
(`discovery_batch_nodes`, `discovery_batch_cost`, checked by `static_assert`).
Paging: a pull request with more than one page of any of the three
connections is read the REST way, which pages as before. Fallback: the
capability probe covers only review threads, viewed files and thread
resolution, none of this query's fields, so the server is learned at run
time, as the existing merge-assessment and review-request reads already are:
a failure of the query other than for the whole session (an older server
without a field, a pull request GitHub no longer has, a malformed answer)
sends the batch the REST way, and an `undefinedField` answer marks the
profile in the provider so later reads skip the query; a failure of the whole
session fails the read as its first REST request would have. The host applies
each row as a single read's, fails a pull request left out alone, and fails
the answer for a row it did not ask for. The fixture server answers the query
for its discovery pull requests; #17, the recorded review, is answered as
having more reviews than a page, so it alone is read the REST way, and a
fixture without discovery data answers as a server without the fields.

| Course refresh | Before | After 3 | After 4 |
|---|---|---|---|
| explicit Refresh (2 repositories, 4 retained): reads, requests | 8, 19 | 6, 11 | 3, 6 |
| restart, 4 repositories, 3 refused | 12, 23 | 14, 19 | 11, 14 |

On a real server each retained pull request cost about six REST requests
(the pull request, reviews, review requests, merge assessment, check runs,
commit statuses); ten now cost one GraphQL request. The owner's example of
20 retained pull requests across five repositories goes from about 120
requests to five or fewer.
