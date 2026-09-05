---
schema: "implement-rfc.card.v1"
id: "BPR-0144"
title: "Confirm review identity on a real account"
status: "Parked"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0142","BPR-0610","BPR-0613","BPR-0614","BPR-0615"]
parked_on: "awaiting-live-github"
---

# Confirm review identity on a real account

RFC-BPR-0008 requires that reviews appear under the authenticated user's
identity with ordinary GitHub semantics. BPR-0137 proves the acting identity is
*carried* into the request. Whether GitHub attributes the review to that
account is GitHub's behaviour, not this project's, and only a real account can
show it.

## Exactly what is missing

BPR-0610 completed the adapter wire contract; BPR-0613 completed separate core
policy and application requests/receipts. BPR-0614 completed provider execution
with durable indeterminate recovery at `f2d23e0f`. BPR-0615 connects the async
host, normal PR-level controls, exact one-use user-submit authority, SQLite
pending restoration and production provider selection. Its local public fixture
covers Comment, empty Approve and Request Changes under the exact synthetic
account, including reconstruction at a newer head without another submission.
The remaining work here is GitHub's real attribution for all three dispositions.

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

## Next attended course

First identify a disposable PR authored by someone other than MikeMarcin while
preserving the chosen reviewer account. Then prepare the exact built application,
current head, account and the three visible review summaries for the existing
attended workflow. No live whole-review approval is inferred from the earlier
source-line comment or mixed-client edit approvals. No live mutation ran in
BPR-0615. The owner has been asked to identify a suitable disposable PR.

The owner requires local verification and no paid CI. Before proposing any live
review or comment operation, inspect the designated target's relevant workflow
triggers; do not execute an operation that would start GitHub Actions. Do not push
or dispatch a workflow. The local fixtures require neither credentials nor CI.
