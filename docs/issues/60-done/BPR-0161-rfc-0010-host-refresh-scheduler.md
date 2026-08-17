---
id: BPR-0161
title: Host: refresh scheduler, limits, offline state, and identity isolation
status: Done
rfc: RFC-BPR-0010
depends_on: [BPR-0158]
---

# Host: refresh scheduler, limits, offline state, and identity isolation

A local scheduler keeps the queue current with conditional requests, the host's
rate-limit state, bounded concurrency, backoff, and cancellation. It runs while
the desktop session permits and installs no system service.

Rate limits are simulated. The RFC says so, which is what makes this slice
entirely testable on this host.

## The failure this slice is really about

A queue refreshes many pull requests at once, so it has more ways than any
earlier surface to deliver a completion into the wrong one. Every completion
carries host, account, repository, pull-request, and generation identity and is
refused on mismatch — by identity, not by arriving in a lucky order.

## Required direct evidence

- Simulated primary and secondary rate limits produce bounded backoff and a
  queue state a reader can act on, not a silent stall.
- Conditional requests reuse their validators, and an unchanged response costs
  no further work.
- Offline state is labelled as such; nothing implies freshness.
- Cancellation is honoured mid-flight, and the cancelled work publishes nothing.
- A completion delivered after an account, repository, or pull-request switch is
  refused, and the test makes the window real rather than describing it.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## What was done

`application::queue_refresh_scheduler` holds no provider and no clock. The host
drives it: it says what to ask for, and it is told what came back.

### Decisions worth stating

**Backoff is a number of seconds, not a moment to wait until.** A moment would
need a clock to compare against, and the scheduler has to stay replayable — the
same answers in the same order must give the same state, whenever they arrive.

**A session is host, account, and generation.** Generation alone is not enough:
an account switch that reused the generation would let one account's answers
land under another's authority. Generation is the host's counter and never a
clock, because two sessions started in the same millisecond must still be
distinguishable.

**Identity is checked before anything else is read.** A stale completion must
not be able to change the validator, the backoff, or the online state either —
those belong to the session that is current, and an answer from a session that
is gone knows nothing about it. Moving the check after the status switch fails
the test.

**Switching account discards every validator.** Carrying an entity tag across
would send it under a different authority, and a "not modified" answer would
then be about a pull request this account may not even be able to read.

**A primary rate limit and a secondary one are distinct statuses.** GitHub means
something different by them — "you have used your allowance" against "slow
down" — and a log that reported one word for both would send the reader to the
wrong place.

**Cancellation is not a refusal.** It publishes nothing and must not push the
scheduler into backoff, or a reviewer switching quickly between reviews would
slow their own queue down.

**Losing permission or being transferred away is `withdrawn`, explicitly.** A
pull request that simply vanished would leave the reviewer wondering whether
they imagined it.

**The provider's `Retry-After` is preferred over the doubling.** It knows when
its allowance resets and the scheduler does not.

### Evidence

Thirteen tests. Six breaks, each landing on its own assertion:

| Break | First failure |
|---|---|
| session compared on generation only | `an answer for another account is refused even at the same generation` |
| the stale check runs after the status switch | `the answer to the old session's question is refused` |
| validators survive an account switch | `and is gone once the account changes` |
| backoff is not capped | `and never longer than the cap` |
| cancellation backs off like a refusal | `and is not treated as the host refusing` |
| losing access drops the item silently | `the item is withdrawn rather than quietly dropped` |

### On the simulated limits

The RFC specifies simulated rate limits, so every one of these runs against
constructed answers rather than a live host. What is proven is what the
scheduler does with each answer. Whether GitHub sends those answers under the
conditions documented is GitHub's behaviour, and no fixture settles it.
