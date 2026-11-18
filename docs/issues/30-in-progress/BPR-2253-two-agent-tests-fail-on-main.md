---
schema: "implement-rfc.card.v1"
id: "BPR-2253"
title: "qt_workbench_agent_send_refusal and application_native_agent_sessions fail on main"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"reservation":"docs/issues/_reservations/BPR-2253--BPR-2253--18fec1e7b72b.json","origin":"seen while building BPR-2252, 2026-11-17"}
---

# qt_workbench_agent_send_refusal and application_native_agent_sessions fail on main

While building [[BPR-2252]], the implementer ran the wider
`agent|tour_chat|workbench_window_scale` set. 120 of 122 tests passed. Two
failed, and they also fail the same way on c6a27facc with BPR-2252's change
reverted, so BPR-2252 did not cause them:

- `qt_workbench_agent_send_refusal`
- `application_native_agent_sessions`

The likely sources are today's lane changes: BPR-2241 added turn seconds,
BPR-2245 changed the queue, BPR-2251 and BPR-2250 changed the history
executor, and BPR-2248 added the commit asks.

## Acceptance

- Find the commit that broke each test. Fix the code, or the test if its
  expectation is stale.
- Both tests pass at -j4.
