---
schema: "implement-rfc.card.v1"
id: "BPR-2052"
title: "Keep agent finding cards when the thread is shown again"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2052--BPR-2052--9ca2282b7544.json","source_commit":"f86732e44"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f86732e44","evidence":{"kind":"command","detail":"qt_agent_finding_cards_retained: four findings, second drafted, then native history re-shown, saved thread re-shown, thread switch and back, later turn: 1 Drafted and 3 open each time. Failed before the fix with 0 cards."}},{"criterion":"AC2","outcome":"pass","revision":"f86732e44","evidence":{"kind":"command","detail":"A restored open card still delivers its own finding; qt_workbench_window_scale_100/125/200, qt_workbench_agent_send_refusal, qt_composed_github_scale_100, format and tidy pass."}}]
---

# Keep agent finding cards when the thread is shown again

Owner report: an agent answer offered four suggested draft comments; after adding
one, the others were gone later. After a turn ends the invocation host clears its
restore key, so the next epoch refresh reloads native history and
`agent_tab_view::present_thread` erased every turn's cards without rebuilding
them.

## Acceptance

- AC1. After one of several finding cards is drafted, re-showing the thread
  (history reload, saved thread, switch away and back, later turn) keeps the
  drafted card as Drafted once and the others open.
- AC2. A restored open card still adds its own finding as a draft.

Not covered: cards across an application restart (needs stored card state and
decoding the stored findings block).
