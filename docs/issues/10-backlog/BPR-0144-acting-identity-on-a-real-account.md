---
id: BPR-0144
title: Confirm review identity on a real account
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0142, BPR-0610]
---

# Confirm review identity on a real account

RFC-BPR-0008 requires that reviews appear under the authenticated user's
identity with ordinary GitHub semantics. BPR-0137 proves the acting identity is
*carried* into the request. Whether GitHub attributes the review to that
account is GitHub's behaviour, not this project's, and only a real account can
show it.

## Exactly what is missing

Production currently hardcodes Comment and creates a source-line comment.
The core disposition enum alone does not prove whole-review submission.
BPR-0610 owns the initial adapter wire contract; subsequent bounded work must
add review-specific policy, receipts and indeterminate recovery, select it in
the provider and composition, and expose whole-review controls before this
card can run its attended checks.

GitHub does not allow authors to approve their own pull requests. The current
MikeMarcin account authored BPR-Test#1, so an authorized different reviewer
or a separately designated PR by another author will be required for the
Approve check. This does not block offline implementation.
[GitHub approval rules](https://docs.github.com/en/pull-requests/how-tos/review-pull-requests/approving-a-pull-request-with-required-reviews).

The designated repository and a token belonging to a **real account** whose
login is known in advance, so the observed author can be compared to it rather
than merely being non-empty.

## Required direct evidence

- A review submitted as Comment, Approve, and Request Changes each appears
  under the expected login.
- The observed author is compared to the active profile's account rather than
  checked for presence.
- No BetterPullReview-specific metadata accompanies the review.
