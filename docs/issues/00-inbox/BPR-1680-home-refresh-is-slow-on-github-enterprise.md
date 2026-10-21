---
schema: "implement-rfc.card.v1"
id: "BPR-1680"
title: "Home refresh is slow on GitHub Enterprise"
status: "Inbox"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner report, 2026-10-20: Home takes a long time to refresh on a GHE environment; anonymized diagnostic export supplied out of band (not committed)","reservation":"docs/issues/_reservations/BPR-1680--BPR-1681--3313b46dcf00.json","related_cards":["BPR-1681","BPR-1679","BPR-0988","BPR-0989","BPR-0990","BPR-1370","BPR-1379","BPR-1436"],"needs":"owner decision on which option to take; each is a design change"}
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

## Acceptance

To be written once an option is chosen. At least: a Home refresh's requests
and duration appear in an anonymized export, and the chosen change reduces
the request count of a fixture refresh with several repositories and retained
pull requests by a stated amount without changing what Home lists.
