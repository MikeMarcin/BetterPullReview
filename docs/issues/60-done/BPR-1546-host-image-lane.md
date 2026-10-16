---
schema: "implement-rfc.card.v1"
id: "BPR-1546"
title: "Host lane, ports and memory cache for inline images"
status: "Done"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1545"]
metadata: {"mandate_revision":"inline-markdown-images-r1","milestone_plan":{"id":"inline-markdown-images-v1","path":"docs/rfcs/plans/inline-markdown-images-v1/milestone-plan.json"},"milestone":"image-host-lane"}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"294025528060badf8ca0243b22cc99ef319dd3ad","evidence":{"kind":"command","detail":"ctest application_markdown_image_host (manual executors, fake fetch/decoder/resolution): four-fetch concurrency and FIFO pump, shared fetch per key, decode only on the decode lane, cancellation of one/all subscribers with late completions releasing slots, SVG/non-image/over-size/blocked/decoder refusals, LRU bound eviction, cache hit without request and never inline, shutdown silence and quiescence; window: same-turn batching, 25-node batches, 128-entry queue bound, stale/absent/unavailable bodies, supersession by a new body request and by scope change (window/session/identity/account/PR), resolution failure, GHES credential hop. Negative control: removing body supersession fails 'a body's new request cancels its earlier load'. application_github_window_read_host still passes."}},{"criterion":"AC02","outcome":"pass","revision":"294025528060badf8ca0243b22cc99ef319dd3ad","evidence":{"kind":"command","detail":"markdown_image_projection carries context, body identity, ordinal, the source as written, state and decoded pixels only; the test asserts no served/signed address reaches it. clang-format 22 and clang-tidy clean on touched files."}}]
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
