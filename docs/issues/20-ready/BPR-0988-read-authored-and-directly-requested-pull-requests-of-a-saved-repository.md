---
schema: "implement-rfc.card.v1"
id: "BPR-0988"
title: "Read authored and directly requested pull requests of a saved repository"
status: "Ready"
rfcs: ["RFC-BPR-0026"]
depends_on: ["BPR-0598"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0026-planning-reconciliation","criteria":["AC-03","AC-05","AC-06"]}
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
