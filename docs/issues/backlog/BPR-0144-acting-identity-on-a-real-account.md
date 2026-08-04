---
id: BPR-0144
title: Confirm review identity on a real account
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0142]
---

# Confirm review identity on a real account

RFC-BPR-0008 requires that reviews appear under the authenticated user's
identity with ordinary GitHub semantics. BPR-0137 proves the acting identity is
*carried* into the request. Whether GitHub attributes the review to that
account is GitHub's behaviour, not this project's, and only a real account can
show it.

## Exactly what is missing

The designated repository and a token belonging to a **real account** whose
login is known in advance, so the observed author can be compared to it rather
than merely being non-empty.

## Required direct evidence

- A review submitted as Comment, Approve, and Request Changes each appears
  under the expected login.
- The observed author is compared to the active profile's account rather than
  checked for presence.
- No BetterPullReview-specific metadata accompanies the review.
