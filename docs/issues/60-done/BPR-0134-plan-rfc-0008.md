---
id: BPR-0134
title: Plan RFC-BPR-0008 verification and allocate its cards
status: Done
rfc: RFC-BPR-0008
depends_on: []
---

# Plan RFC-BPR-0008 verification and allocate its cards

RFC-BPR-0008 makes BetterPullReview's interactions round-trip through the
ordinary pull request: reviews, threads, replies, viewed state, and
resolution, read and written under the user's own identity.

## Outcome

`docs/verification/RFC-BPR-0008.md` records the allocation, a nine-row claim
matrix, the deferred gates, and cards BPR-0135 through BPR-0141.

**This is the first RFC in the pack with gates the product owner must open.**
Three acceptance criteria cannot be met on this host, and each is carded with
exactly what is missing rather than with a general note that credentials are
needed:

- **BPR-0142** — a comment readable and replyable in the GitHub web UI with no
  tool-specific marker. A fixture proves what BetterPullReview sent; only
  GitHub shows what GitHub made of it, and only a person can confirm the page
  carries no marker.
- **BPR-0143** — mixed-client edit, delete, resolve, and reopen. The fake
  provider proves the projection handles each shape; it cannot prove GitHub
  produces them.
- **BPR-0144** — reviews appearing under the authenticated identity. The
  fixture proves the identity is *carried*; whether GitHub attributes the
  review to that account is GitHub's behaviour.

All three need a designated safe repository and a write-scoped token.
**Nothing in `docs/` designates one today**, and no slice here will invent one.
The cards say so plainly and say what must not be done instead — in particular,
that weakening the coordinator's safe-target check to let a test through would
remove the property the test exists to exercise.

## The safety rule comes first

"No automated test can mutate a non-designated live repository/PR" is an
acceptance criterion, and it is the one that must hold before any mutation code
is trusted. It is not satisfied by intending to be careful.

Every implementation slice runs against the fixture provider. The coordinator
refuses an undesignated target, and BPR-0137 must prove that refusal for a
target altered *after* the prepare check — the case a check performed only at
prepare time would miss.
