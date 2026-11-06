---
schema: "implement-rfc.card.v1"
id: "BPR-1892"
title: "A Tour tab agent request shows its failure on the Tour tab"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-06, coordinator chat: 'It's also weird that I have to switch to the agent tab to see my tour agent errors'","related_cards":["BPR-1878","BPR-1881","BPR-1891"]}
acceptance: []
---

# BPR-1892: A Tour tab agent request shows its failure on the Tour tab

## Owner report (2026-11-06)

"It's also weird that I have to switch to the agent tab to see my tour agent
errors."

When the Tour tab's agent drafting fails (Draft from PR context, Enhance,
Generate comment, Suggest an outline, or tour message writing), the failure
card appears only on the Agent tab. The author has to leave the tour to learn
that anything went wrong, and to find out what to do about it.

## Owner direction (2026-11-06, relayed by the coordinator)

"It's probably fine for the errors to stay in the agent tab, but we should at
least make the tour error a) stand out more and b) send you there."

This replaces the first draft of this card, which put the Agent tab's failure
card on the Tour tab with an in-place Retry.

## Change

- The failure card stays on the Agent tab. It is not reproduced on the Tour
  tab.
- On the Tour tab, a failed agent request shows a clearly visible notice next
  to the control that started it (the Problem or Solution helper, the stop's
  comment helper, or Suggest an outline): one short line saying the request
  failed, in the Agent tab's own words for that failure
  (`agent_tab_view::run_failure_words`, no second copy), and a button that
  opens the Agent tab at that failed run's card.
- The notice clears when its request is made again or succeeds, and it can
  be dismissed.
- Nothing switches tabs by itself.

## Acceptance

- AC1. A failed Tour tab agent request shows a notice next to its control on
  the Tour tab, without a tab switch: one line saying it failed, in the words
  the Agent tab uses for that failure.
- AC2. The notice's button opens the Agent tab at the failed run's card.
- AC3. The notice clears when its request is made again or succeeds, and
  Dismiss hides it. Successful requests show nothing new.
- AC4. Owner look: attended Accept/Reject on a frozen build that opens on a
  Tour tab showing a failure.
- AC5. `python3 scripts/verify_documentation.py --root .` passes.
