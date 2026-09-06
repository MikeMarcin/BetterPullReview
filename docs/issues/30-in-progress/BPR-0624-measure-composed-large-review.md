---
schema: "implement-rfc.card.v1"
id: "BPR-0624"
title: "Measure large-review interactions through the composed application"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0623"]
---

# Measure large-review interactions through the composed application

Serve the checked-in BPR-0623 workload through the ordinary GitHub provider,
installed Git and application selection path. Establish a measured local
baseline before selecting performance budgets. RFC base remains
`d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; BPR-0617 is the closeout anchor.

## Bounded acceptance and ownership

- Add a test-only large-review fixture that exhausts six REST file pages and
  the corresponding GraphQL file/thread pages. Assert all 515 files and
  1,000 source threads/comments, exact authority/head, script exhaustion and
  installed-Git transport selection. Keep existing strict single-page fixture
  validation intact; paged replay must match exact compiled requests and let
  the production pager validate response/continuation contracts.
- Exercise real application file selection, display the exact 20,000-line
  comparison, navigate changes and scroll. Render a full application window
  and verify exact displayed source identities. Exercise rapid selection
  supersession and verify the final selection wins after background work.
- Record elapsed load/selection/gesture timings, event-loop observations and
  host/workbench counters with platform, architecture, Qt, scale, input and
  fixture-size context. Distinguish measured work overlap from idle gestures.
  Do not turn this machine's measurements into universal latency claims.
- Own the test-only fixture/composition selection under `src/app/`, a focused
  benchmark course and registration under `tests/end_to_end/`, any required
  large-review wire fixture data/helper, and this card/closeout. Keep provider
  production contracts unchanged unless this selected path exposes a defect.
- Run the focused composed course, a retained single-page application lane,
  fixture contract, isolation manifest, documentation gate and diff check.
  All verification is local and network-isolated; no GitHub Actions, paid CI,
  push, credential use, external AI process or production application launch.

This slice measures the latest head. The pack's 32 heads do not establish
32 application epochs. Memory/cache retention, platform-native budgets,
broader RFC resilience criteria and Windows/Linux runtime evidence remain
open. Record any measured defect and its remaining scope truthfully.
