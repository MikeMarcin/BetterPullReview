---
schema: "implement-rfc.card.v1"
id: "BPR-1758"
title: "Pin mimalloc and select it for BPR product executables"
status: "Done"
rfcs: ["RFC-BPR-0093"]
depends_on: []
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"81ee1b4efc5523d3bc1ee42e5e3d61d323af76c9","evidence":{"kind":"command","detail":"Verified pinned archive and licence; offline source/collision/corruption controls and extraction restoration pass.","refs":["docs/verification/RFC-BPR-0093.md","build/mimalloc-m1/provider-tests.log","build/mimalloc-m1/actual-graphs.json","build/mimalloc-m1/baseline-result.txt"]}},{"criterion":"AC02","outcome":"pass","revision":"81ee1b4efc5523d3bc1ee42e5e3d61d323af76c9","evidence":{"kind":"command","detail":"All selected product targets attach shared mimalloc; actual core-only and baseline graphs omit it, baseline bpr runs, minimal distribution omits absent families, baseline distribution refusal and instrumentation exclusions pass.","refs":["docs/verification/RFC-BPR-0093.md","build/mimalloc-m1/provider-tests.log","build/mimalloc-m1/actual-graphs.json","build/mimalloc-m1/baseline-result.txt"]}}]
---

# Pin mimalloc and select it for BPR product executables

## Scope and acceptance

Implement the RFC's pinned source/provider and executable policy (AC01–AC02).
The selected product graph includes desktop, optional `bpr`, MCP helper,
supervisor and macOS update helper according to their existing platform and
family switches. Keep mimalloc out of
core and other layer contracts, and keep a core-only graph allocator-free.
Provide the non-distribution comparison switch,
make distribution refuse that switch and preserve offline/substitution controls.
Record the exact build graph and negative configure observations.
