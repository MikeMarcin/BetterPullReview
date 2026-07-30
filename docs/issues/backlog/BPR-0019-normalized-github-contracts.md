---
id: BPR-0019
title: Implement normalized GitHub read contracts
status: Backlog
rfc: RFC-BPR-0002
depends_on: [BPR-0018]
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
