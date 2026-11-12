---
schema: "implement-rfc.card.v1"
id: "BPR-2120"
title: "Give composed terminal-diagnostic children a bounded Debug watchdog"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2120--BPR-2120--94c21bf5d692.json","source_baseline":"70324127b116e45988b8ffa3a77370209ba7aef4","branch_baseline":"000b2ad0b70b760d4e366f8e94709300ceb9ae96","mandate_revision":"release58-debug-terminal-watchdog-r1","milestone_plan":{"id":"bpr-2120-debug-terminal-watchdog","path":"docs/rfcs/plans/bpr-2120-debug-terminal-watchdog/milestone-plan.json"}}
---

# Keep the child watchdog outside its functional waits

The terminal contract gives every subprocess ten seconds, including composed logging children whose own condition waits permit fifteen seconds and whose standalone CTest rows permit thirty. Mac58 Debug was killed at the ten-second wrapper backstop; later recovery and pipe assertions were not reached.

Keep all Release limits unchanged. In Debug only, permit thirty seconds for the three composed logging children and 180 seconds for the complete ten-child course. All other child limits stay ten seconds, and all eighteen assertions, original C++ waits, sandbox and environment stay intact.

## Acceptance

- AC1: Preserve the original cutoff, interrupted full Debug scope and exact one bounded diagnostic result, without claiming causal latency reproduction or a full-suite pass.
- AC2: Explicit configuration selects Debug composed-child30/other10/outer180 and Release child10/outer60. Execute the actual run function with configuration/child controls and an old-bound negative; preserve the original course body and assertions. Bind the already completed exact-input functional probe honestly.
- AC3: Scoped source/configuration/documentation checks and intentional main contribution preserve foreign work, counters and the frozen producer; return exact private/canonical mappings and terminal ownership state.
