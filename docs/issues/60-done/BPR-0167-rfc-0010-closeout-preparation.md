---
schema: "implement-rfc.card.v1"
id: "BPR-0167"
title: "Prepare RFC-BPR-0010 closeout"
status: "Done"
rfcs: ["RFC-BPR-0010"]
depends_on: ["BPR-0158"]
---

# Prepare RFC-BPR-0010 closeout

Bring `docs/verification/RFC-BPR-0010.md` to a state a reviewer can audit: every
claim-matrix row resolved to Verified, Partially verified, or Deferred with the
card that carries it, and the deferred gates listed with exactly what is
missing.

RFC-BPR-0010 stays `Proposed`. Closure is ordered behind RFC-BPR-0002 and this
card makes no closure claim.

## Required direct evidence

- No row reads Unstarted.
- Every Verified row names the card and the command that proves it.
- Every Deferred row names what is missing and who must supply it.

## What was done

`docs/verification/RFC-BPR-0010.md` is auditable. No claim-matrix row reads
Unstarted: eleven rows, each naming the card that carries it and what the
evidence actually was.

### The replay

Every command and its result, run at `8903b4d` on macOS 15.7.7 arm64 with
Qt 6.11.1 — dev 118/118, core-only 53/53, format, clang-tidy, and four
validators — plus the six RFC-BPR-0010 suites and which preset each runs
under.

Both core suites run under `core-only`, which links neither Qt nor SQLite. That
makes "the core reads no clock and reaches no provider" a property of the build
rather than of a review.

### One row is honest rather than green

*Enrollment survives the path it matched* reads **verified for the retention
policy**, not verified. BPR-0162 built the policy and no store. Writing it as
verified would have been the easy thing and would have made a reader believe an
enrollment survives a restart, which it does not.

### Remaining work is listed apart from the gates

BPR-0170 — the durable store — needs no Windows host, no person, and no
credential. It is unbuilt, not blocked. Filing it under "deferred gates" beside
the attended notification check and the live-repository check would have made
this RFC look more finished than it is, so it has its own section saying
exactly that.

Filters and sorting are recorded there too, as named by BPR-0163's card and not
built.

## What this card does not do

Make RFC-BPR-0010 `Auto-Verified`. Closure is ordered behind RFC-BPR-0002 and
no card here makes a closure claim.
