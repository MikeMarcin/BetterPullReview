---
schema: "implement-rfc.card.v1"
id: "BPR-1594"
title: "Agent is a drawer tab"
status: "Done"
rfcs: ["RFC-BPR-0085"]
depends_on: ["BPR-1592"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "agent-tab", "criteria": ["AC07"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
acceptance: [{"criterion":"AC07","outcome":"pass","revision":"21030cfb4","evidence":{"kind":"observation","detail":"Merged origin/main 3b68c4394 after BPR-1593. All 16 focused rows pass serially (235.57 s): three window scales, keyboard scales, side chat, Agent panels/canaries, finding queue, Notes, finding public path, architecture and both composition contracts. The touched dif-context public course also passes (8.90 s). Printed round-trip pins match, dated merged-tree review/reseal recorded. Format22/docs/diff pass."}},{"criterion":"owner-agent-tab","outcome":"pass","revision":"cd19a8988","evidence":{"kind":"observation","detail":"Owner accepted frozen-cd19a8988-v001 without a note in session 5547b2341ac4b1228c7f89e5ed5110c1, sequence 4; handback 5 consumed, protection released at 6. Known visual gaps retained on checkpoint; no new design introduced."}}]
---
# BPR-1594: Agent is a drawer tab

RFC-BPR-0085 D5: the Agent tab with ⌥⇧A, ⌘I and "Ask about these lines";
the context row, conversation with finding cards (their promote action
reads **Draft comment**, the owner's choice at [[BPR-1588]]'s review), question field, History
(sessions, earlier heads, private runs) and the four states.

## Acceptance

- RFC-BPR-0085 AC07; the agent composition contracts pass.
- Owner, attended: "The agent has its own tab and is one key away."

## Boundaries

Qt presentation and keys over the existing agent hosts; read-only authority
unchanged.
