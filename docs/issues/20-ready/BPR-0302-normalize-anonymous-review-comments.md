---
id: BPR-0302
title: Normalize anonymous REST review comments and unavailable state
status: Ready
rfc: RFC-BPR-0017
depends_on: [BPR-0300, BPR-0301]
---

# Normalize anonymous REST review comments and unavailable state

## Required final state

- One REST review-comment operation is added to the existing closed read
  contract, paging, fixtures, provider, and normalizer; no parallel path exists.
- Reply linkage reconstructs comment grouping without inventing resolution;
  viewer, viewed state, resolution, and actionability remain explicitly absent.
- The normalizer admits the exact anonymous operation set instead of requiring
  ten responses or hardcoding a full read shape.
- Malformed/cyclic/missing linkage, paging, 404 neutrality, rate limits,
  oversized bodies, and authenticated non-regression have direct tests.

## Lane — 2026-09-02

[[BPR-0300]] and [[BPR-0301]] are terminal, so this card moves to
`20-ready/` as generic work runnable on any station. What they left at this
card's door: `perform_paged_github_read` now takes a
`github_provider_read_authority` and `provider_request_valid` requires the
account transport only under the authenticated authority, so the pager edit
this card makes lands on top of that; an anonymous `fetch_pull_request`
refuses any read shape that is not all-false, which is the shape this card's
availability-aware normalization must produce for the anonymous case; and
`github_anonymous_read_contract` observes only one eligible operation on the
wire because no anonymous snapshot corpus exists yet — the REST
review-comment operation and its fixtures, which this card owns, are where
that corpus starts. Core's `inline_review_comment_observation` already
distinguishes unobservable from observed-none, so the normalizer has a typed
target for "not observable" rather than an empty list.
