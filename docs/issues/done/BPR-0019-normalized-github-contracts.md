---
id: BPR-0019
title: Implement normalized GitHub read contracts
status: Done
rfc: RFC-BPR-0002
depends_on: [BPR-0018]
completed: 2026-07-29
---

# Implement normalized GitHub read contracts

Define bounded provider requests, normalized viewer, repository, pull-request,
file, commit, review, review-thread, issue-comment, check, search-result,
capability, rate-limit, and failure records, plus the narrow application
provider port. Cover `GetViewerIdentity`, `GetRepository`, `GetPullRequest`,
`ListPullRequestFiles`, `ListPullRequestCommits`, `ListReviews`,
`ListReviewThreads`, `ListIssueComments`, `ListChecks`, and
`SearchPullRequests`. Add equivalent sanitized GitHub.com and GHES 3.18
recorded contracts that reject unexpected paths, headers, queries, fields, and
operations.

Provider JSON, headers, URLs, tokens, and transport errors must not cross into
the core or Qt adapter.

## Result

- Added bounded standard C++ records for viewer, repository, pull request,
  files, commits, reviews, review threads, issue comments, checks, search
  results, capabilities, rate limits, and typed provider failures.
- Added a narrow asynchronous application provider port whose success,
  failure, cancellation, and request contexts retain exact authority.
- Added canonical, injective request compilation for the ten selected read
  operations, exact REST and GraphQL allowlists, and mutation rejection.
- Added one global GitHub node-identity collision domain across repositories,
  pull requests, commits, reviews, threads, comments, checks, actors, and
  non-target search results.
- Added fail-closed enum, relationship, text, record, JSON, GraphQL signed
  integer, allocation, and aggregate limits. Unknown or continued page
  completeness cannot produce a snapshot.
- Added paired sanitized GitHub.com and GHES 3.18 contract manifests whose
  selected projections normalize to one independent semantic expectation.
- Preserved the target-native build policy: no GitHub Actions workflow or
  hosted result participates in building or verifying this slice.

Production raw-envelope handling and exhaustive REST/GraphQL pagination
remain explicitly assigned to BPR-0027. The one-page recorded facade cannot
be used to claim that a larger live pull request loaded completely.

## Evidence

- Qt-enabled `dev` matrix: 20/20 passed.
- Qt-free `core-only` matrix: 14/14 passed.
- Architecture contract checks: 22/22 passed.
- Fixture security checks: 10/10 passed.
- Platform configuration checks: 4/4 passed.
- Documentation and original source-pack validators passed.
- `git diff --check`: passed.
- GitHub Actions workflow files and tracked workflow files: zero.
