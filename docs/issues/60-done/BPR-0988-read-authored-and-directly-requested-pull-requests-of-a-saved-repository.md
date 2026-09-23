---
schema: "implement-rfc.card.v1"
id: "BPR-0988"
title: "Read authored and directly requested pull requests of a saved repository"
status: "Done"
rfcs: ["RFC-BPR-0026"]
depends_on: ["BPR-0598"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0026-planning-reconciliation","criteria":["AC-03","AC-05","AC-06"]}
acceptance: [{"criterion":"github_read_contract tests compile the exact search and requested-reviewers requests for GitHub.com and GHES 3.18 and refuse any other search terms, qualifier or login shape","outcome":"pass","revision":"8d1898e7","evidence":{"kind":"command","detail":"github_read_contract passes with test_personal_discovery_requests: exact /search/issues and /api/v3/search/issues queries for author: and review-requested:, exact requested_reviewers paths; 14 hostile/foreign term shapes, overlong login, page 2, numbered search, unnumbered detail, detail with terms and an extra outbound query parameter refused; catalog unchanged at ten."}},{"criterion":"github_paged_read and github_provider tests over exchanges: complete multi-row answer, incomplete_results, total above the bound, author mismatch dropped, team-only request dropped, direct request kept, awaited reviewers counted, primary and secondary limits with retry-after, 401/403/404/422 normalized, and cancellation","outcome":"pass","revision":"8d1898e7","evidence":{"kind":"command","detail":"github_paged_read passes with test_personal_discovery_read (authored 3/0 awaited, other actor dropped; requested kept only when viewer named; incomplete; 100-row next page result_limit with 30 detail reads; 31 requested detail_limit; GHES paths; 403 primary search limit, secondary limit with retry, 401 authentication, 403 and 422 and detail 404 as permission; cancellation and invalid authority send nothing). github_provider passes with test_personal_discovery_entry (one job, one terminal, four forged authorities refused before effect, cancellation). Negative controls for author normalization and direct-request check fail their assertions."}},{"criterion":"A recording transport shows that a discovery read issues only search and requested-reviewers requests for the named repository","outcome":"pass","revision":"8d1898e7","evidence":{"kind":"command","detail":"discovery_fixture_transport asserts on every request that it passes production validate_github_wire_request, names octo-org/review-repository, and is either the repository-scoped search or a requested-reviewers read under that pull request's context; counts show one search plus one detail read per kept row and none for dropped actors."}},{"criterion":"Changed-surface upkeep: authority checks in the transports, boundaries, format, tidy, documentation","outcome":"pass","revision":"8d1898e7","evidence":{"kind":"command","detail":"native_https_transport_contract refuses a repository-scoped search with another account or host (negative control narrowing the check fails discovery-authority-drift-preflight); gh_cli_transport compiles both reads to the exact closed invocation; 27 focused selectors pass (build/bpr0988-focused.log); core-only preset core suites pass; architecture_boundaries passes on a clean export; clang-format on 22 files; tidy clean on changed lines; github-host-implementation.md and adapter AGENTS.md updated; DOCUMENTATION_OK; git diff --check clean."}}]
---

# Read authored and directly requested pull requests of a saved repository

## Scope

The provider half of RFC-BPR-0026's personal discovery. At planning head
`2722bd67` the only pull request search is the repository-wide `is:open`
search inside the full snapshot read (`github_paged_read.cpp`, the
`search_pull_requests` stage; `safe_search_terms` admits `is:open` alone,
`github_read_contract.cpp:398-401`), and the queue learns about a pull
request only from a snapshot the reviewer opened, with no GitHub role
(`queue_observation_for`, `github_application.cpp:757-786`). Nothing can
list the reviewer's own or requested pull requests without opening them.

- **One bounded discovery read per saved repository and reason**,
  independent of the snapshot read: REST issue search for open pull
  requests in exactly that repository authored by, or with a review
  requested of, the accepted viewer login. The qualifiers are the ones the
  GitHub.com and GHES 3.18 server contracts both document (`author:`,
  `review-requested:`); no `@me`, `user-review-requested:` or other
  web-only qualifier is assumed. Pages and results are bounded;
  `incomplete_results` or a total above the bound makes the answer partial,
  never complete.
- **Identity normalization.** A result whose author is not the viewer is
  not an authored result. A review-requested result is kept as a direct
  request only when the pull request's requested reviewers name the viewer
  as a user; a team-only request is dropped (automatic team-request
  discovery is a non-goal). The same bounded requested-reviewers read gives
  an authored pull request its awaited-reviewer count, so "waiting on
  others" is known rather than guessed. Those reads are bounded per refresh.
- **Honest failure.** Primary and secondary rate limits on the search
  resource carry their retry-after; offline, authentication, access loss
  and a removed or renamed repository are distinct failures; cancellation
  is a terminal of its own. A failure never reads as an empty result.
- **Nothing heavier.** A discovery read issues no snapshot, file, commit,
  thread, comment, check, timeline or diff request and hydrates nothing.
- **Port.** A narrow application port carries the request (exact accepted
  session, authority, viewer, account transport, repository, reason,
  generation) and the bounded completion (rows with number, title, author,
  reason, direct-request and awaited-reviewer facts, completeness, typed
  failure). No provider type crosses inward.

Owned surfaces: `src/adapters/github` read contract, paged read and
provider; the application port header; recorded GitHub.com and GHES
exchanges under `tests/fixtures/github` or generated fixtures;
`tests/github_contract` read-contract, paged-read and provider tests.

## Acceptance

- `github_read_contract` tests compile the exact search and
  requested-reviewers requests for GitHub.com and GHES 3.18 and refuse any
  other search terms, qualifier or login shape.
- `github_paged_read` and `github_provider` tests over recorded exchanges:
  a complete multi-page answer, `incomplete_results`, a total above the
  bound, an author mismatch dropped, a team-only request dropped, a direct
  request kept, awaited reviewers counted, primary and secondary limits
  with retry-after, 401/403/404/422 normalized, and cancellation.
- A recording transport shows that a discovery read issues only search and
  requested-reviewers requests for the named repository.

## Evidence

Not run at allocation (planning head `2722bd67`).

## Implementation and verification — 2026-09-22

Source commit `8d1898e7` on `main`, claimed at `c44f2f38`.

- **Contract.** `read_operation::search_personal_pull_requests`
  (`GET {rest}/search/issues`, `q=is:pr is:open author:LOGIN repo:OWNER/NAME`
  or `review-requested:LOGIN`, one page of 100) and
  `read_operation::list_requested_reviewers`
  (`GET {rest}/repos/OWNER/NAME/pulls/N/requested_reviewers`), both outside
  the recorded snapshot manifest. `safe_personal_search_terms` admits exactly
  one of the two qualifiers and a login of letters, digits, hyphen and
  underscore (no leading hyphen, at most 64 bytes); outbound validation
  recompiles the canonical request, so neither transport can send another
  shape. Search bills the `search` rate resource.
- **Wire authority.** A new `core::repository_request_context` parent:
  validation admits it for the personal search of exactly its repository and
  for nothing else, and refuses the personal search under a pull request
  parent; the requested-reviewers read runs under its pull request's
  context. Native HTTPS checks host and account for it as for a pull request
  read; an anonymous authority refuses both reads as identity operations.
- **Read.** `perform_personal_discovery_read`: one search page, author
  normalized against the viewer (case-folded), up to 30 requested-reviewer
  reads, a requested row kept only when its users name the viewer, a total
  above the page / a `next` continuation / `incomplete_results` / more
  requested rows than the detail bound reported as partial, duplicates and
  another repository's rows refused, any failed request failing the whole
  answer, and a refused repository qualifier (422) reported as lost access.
- **Port.** `application::personal_discovery_port` on `github_provider`,
  using the exact account lease a pull request read uses, one job, exactly
  one terminal, and refusal before any effect for a foreign account,
  generation, lease or credential provenance. No provider type crosses
  inward.

Checks at the committed tree:

- `github_read_contract` (new `test_personal_discovery_requests`): exact
  paths and queries on GitHub.com and GHES 3.18; fourteen hostile or
  foreign search-term shapes, an overlong login, page 2, a numbered search,
  an unnumbered detail read, a detail read with terms and an outbound query
  with an extra parameter all refused; names round-trip; the recorded
  catalog is unchanged at ten.
- `github_paged_read` (new `test_personal_discovery_read`, scripted
  exchanges asserting every request passes production wire validation and
  names only the saved repository): authored rows kept and detailed (awaited
  3 and 0) with another actor's row dropped at no cost; requested rows kept
  only when the viewer is a named user (team-only and other-user dropped);
  `incomplete_results`; a 100-row page with a `next` link as `result_limit`
  with exactly 30 detail reads; 31 requested rows as `detail_limit`; the GHES
  paths; primary (403, remaining 0) and secondary search limits with retry;
  401, 403, 422 and a failed detail read; cancellation, foreign viewer, no
  lease and a login smuggling a qualifier refused with no request; duplicate
  and foreign-repository rows refused; the wire refusing the search under a
  pull request parent or for another repository and the repository parent for
  any other operation.
- `github_provider` (new `test_personal_discovery_entry`): one posted job,
  one terminal carrying the exact request; four forged authorities refused
  before any effect; cancellation still delivers one terminal.
- `native_https_transport_contract`: a repository-scoped search with another
  account or host is refused before any connection.
  `gh_cli_transport`: both reads compile to the exact closed `gh api`
  invocation on both contracts.
- Negative controls, each restored: without author normalization
  (`build/bpr0988-negative-author.log`), without the direct-request check
  (`build/bpr0988-negative-direct.log`) and with the native account check
  narrowed to pull request parents (`build/bpr0988-negative-native.log`) the
  named assertions fail.
- 27 focused selectors pass (`build/bpr0988-focused.log`): every GitHub
  contract, fixture and transport suite above plus `github_anonymous_read_contract`,
  `native_https_*_fixture`, `github_capability_probe`,
  `gh_cli_git_smart_transport`, `application_github_*`, `core_github_*`,
  `application_github_public_path_github_com`,
  `application_github_public_path_uiforetw_74`,
  `application_setup_destination_public_path` and the core and architecture
  boundary contracts; the core-only preset builds and its core suites pass;
  `architecture_boundaries` passes on a clean export.
- `clang-format` check passes on all 22 touched C++ files; strict tidy
  reports nothing on changed lines (its findings in these files are
  pre-existing lines). `docs/architecture/github-host-implementation.md` and
  the adapter's `AGENTS.md` describe the reads; `verify_documentation.py`
  is `DOCUMENTATION_OK`; `git diff --check` clean.

Limitations: exchanges are scripted in the tests, not recorded from a live
server; whether `review-requested:` behaves identically on a real GHES 3.18
instance is taken from the documented contract, not observed. No live
GitHub or GHES request was made. The change widens the GitHub request
surface (a new parent authority and two request shapes); it was
self-reviewed only — an independent critical review of the request and
authority boundary would add evidence and is not claimed.
