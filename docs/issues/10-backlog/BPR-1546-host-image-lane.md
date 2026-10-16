---
schema: "implement-rfc.card.v1"
id: "BPR-1546"
title: "Host lane, ports and memory cache for inline images"
status: "Backlog"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1545"]
metadata: {"mandate_revision":"inline-markdown-images-r1","milestone_plan":{"id":"inline-markdown-images-v1","path":"docs/rfcs/plans/inline-markdown-images-v1/milestone-plan.json"},"milestone":"image-host-lane"}
---

# BPR-1546: Host lane, ports and memory cache for inline images

The application host resolves, fetches, decodes and caches body images on a
bounded cancellable lane and publishes a provider-free image projection
([RFC-BPR-0084](../../rfcs/RFC-BPR-0084-inline-markdown-images.md)).

## Acceptance

- **AC01:** Host tests with fake ports and the manual executor cover the
  concurrency and queue bounds, cancellation on body, window, sign-out and
  account change, stale and duplicate completions, the memory-cache bound and
  isolation, cache hits making no request, and shutdown with work in flight.
- **AC02:** The projection carries no URL, signed address, header or
  credential.
