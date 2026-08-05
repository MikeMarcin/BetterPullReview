---
id: BPR-0161
title: Host: refresh scheduler, limits, offline state, and identity isolation
status: Backlog
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
