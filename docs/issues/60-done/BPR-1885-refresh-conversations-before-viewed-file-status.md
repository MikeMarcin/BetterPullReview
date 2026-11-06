---
schema: "implement-rfc.card.v1"
id: "BPR-1885"
title: "Refresh conversations before viewed-file status pagination"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1885--BPR-1885--cb56fb646bfe.json","related_cards":["BPR-1883"],"milestone_plan":{"id":"responsiveness-latency-v2","path":"docs/rfcs/plans/responsiveness-latency-v2/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"7578fbfeb6d5d3980327f0eadf7ff2cdde0d8713","evidence":{"kind":"observation","detail":"github_paged_read passes complete conversation before either viewed-file continuation, exact final viewed states for 201 files, equal conversation records and 20 requests, and unchanged first-open/unobserved ordering."}},{"criterion":"AC2","outcome":"pass","revision":"7578fbfeb6d5d3980327f0eadf7ff2cdde0d8713","evidence":{"kind":"observation","detail":"The same course rejects late head/count/cursor/duplicate/missing/state faults, cancels remaining viewed-file pages, suppresses conversation after prior cancellation, and enforces the shared eight-page budget without accepting incomplete snapshots. Provider and window-host courses pass unchanged authority/handoff contracts."}},{"criterion":"AC3","outcome":"pass","revision":"7578fbfeb6d5d3980327f0eadf7ff2cdde0d8713","evidence":{"kind":"observation","detail":"Baseline negative control fails at the targeted early-observation assertion. Serial optimized fixture measurement with two 330 ms continuation waits improves median conversation observation 673.210 to 16.747 ms, retaining 20 requests. Composed public refresh and inspected offscreen render retain comparison and pending markers with zero blank samples or re-wash."}},{"criterion":"AC4","outcome":"pass","revision":"7578fbfeb6d5d3980327f0eadf7ff2cdde0d8713","evidence":{"kind":"observation","detail":"Source, tests, host contract and one-line journal landed on main at this revision. Both changed C++ units pass clang-format/tidy 22, core_dependency_boundary and documentation checks pass. The unrelated architecture_boundaries failure in the BPR-1884 smart transport reproduces on pre-round main and is documented; earlier inherited broader failures are not claimed as passing."}}]
---

# BPR-1885: Refresh conversations before viewed-file status pagination

The owner requested continued direct responsiveness work on 2026-11-06,
especially independent loading, with every round landed on main, a card for
each change and a one-line journal entry. Round one landed at `661981be0`.

The GraphQL pager exhausts viewed-file status pages before completing review
threads. Those statuses do not decide conversation content, but they delay the
same-head refresh observation on pull requests with many changed files.

## Acceptance

- AC1: Eligible authenticated refreshes publish complete reviews and review
  threads before viewed-file pagination; keep first-open and unobserved ordering,
  exact final records and request counts.
- AC2: Exhaust and validate every viewed-file page before accepting a snapshot;
  retain cursor, count, identity, limit and cancellation checks, including a
  failure or cancellation after the conversation is shown.
- AC3: Verify the composed refresh still displays pending work and its comparison;
  record optimized before/after latency and a negative control.
- AC4: Pass focused source/documentation checks, record inherited failures
  accurately, complete the journal/card and land the round on main.

## Evidence

Viewed-file loading is one local operation whose existing parsing, pagination
and validation remain intact. Eligible refreshes finish review threads and
publish the existing conversation observation before invoking it. The initial
GraphQL response still carries the first pages together. First-open and
unobserved reads retain their prior order; the existing host/core admission and
partial-refresh UI remain the owners of authority and loading feedback.

Release, Apple M4 Pro arm64, macOS 26.6.2, AppleClang 21.0.0, Qt 6.12.0,
default test allocator. The provider fixture has 201 changed files, three
viewed-file pages and a 101-comment thread. Only the two viewed-file
continuations sleep for 330 ms each; all other reads are immediate synthetic
responses. Baseline product source is round one's integrated `661981be0`.
The same `github_paged_read --measure-viewed-files` harness runs three rounds
before and after, without concurrent task-owned builds or tests. Times are ms:

| Round | Conversation before | Conversation after | Settled before | Settled after |
|---|---:|---:|---:|---:|
| 1 | 669.976 | 34.210 | 670.194 | 700.970 |
| 2 | 673.309 | 16.747 | 673.490 | 686.392 |
| 3 | 673.210 | 12.987 | 673.373 | 685.528 |

Median provider conversation observation improves by 656 ms, approximately
the two independent page waits. Every round still makes 20 requests. This
isolates provider observation latency; it is not a live-GitHub or whole-app
17 ms claim, and full refresh completion is not faster in these samples.
No Debug timing gate is run or claimed.

- `github_paged_read` passes with the paginated viewed-file fixture: all 201
  final viewed states, conversation records and request counts agree with
  first-open and unobserved reads. Those paths retain viewed-file-before-thread
  ordering. The new regression fails against the old pager at “complete
  conversation is observable before viewed-file pagination returns.”
- The same course rejects late head/count/cursor/duplicate/missing/state errors,
  cancels between viewed-file pages, suppresses an observation when cancelled
  during the last thread page, and retains the shared eight-page budget at the
  conversation boundary. Early content never turns these into an accepted
  replacement snapshot.
- `github_provider` and `application_github_window_read_host` pass, retaining
  exact request identity, same-head/base-only publication, unchanged snapshot
  authority and epoch handoff.
- clang-format 22.1.8, clang-tidy 22.1.8 with warnings as errors on both changed
  translation units, `core_dependency_boundary` and `git diff --check` pass.

The current-main `architecture_boundaries` course fails at the neutral working
directory validation in `gh_cli_git_smart_transport.cpp`, added by BPR-1884.
Running the same verifier directly against clean main `1aa7f4a9f` reproduces
that refusal without this round's source changes. It is recorded as failed;
this round changes neither that file nor its ownership allowlist. The earlier
BPR-1883 architecture-fixture, network-manifest and whole-Qt lint failures
remain recorded; those unrelated broad courses were not repeated here.

The composed `refresh_current_file_public_path` passes with all assertions
at 330 ms per read: its standard small fixture shows the fresh conversation
at 1,665 ms, settles at 6,002 ms, makes 18 reads, and has zero blank comparison
samples or renewed wash after freshness. The full-window offscreen render was
inspected: the comparison and fresh comment count are visible alongside the
“Loading commits…” stage and “Refreshing remaining details…” marker. This
retains BPR-1883's loading feedback; it is not a native-style acceptance claim.
No Qt surface changes are needed for this round.

The final documentation gate passes; its unchanged 67-test regression course
and the unchanged composition inventory/hostile-control check reuse the
passing evidence from round-one integration. Source, tests, host contract and journal landed on main at
`7578fbfeb6d5d3980327f0eadf7ff2cdde0d8713`. This card closes over that landed source.
The [journal](../../development/responsiveness-journal.md) records this round's
one notable improvement.
