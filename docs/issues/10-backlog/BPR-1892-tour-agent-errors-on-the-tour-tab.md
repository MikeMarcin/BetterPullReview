---
schema: "implement-rfc.card.v1"
id: "BPR-1892"
title: "A Tour tab agent request shows its failure on the Tour tab"
status: "Backlog"
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

## Change

- Show the failure on the Tour tab, next to the control or step that started
  the request. It uses the same plain words and next action that the Agent
  tab's failure card uses (one source of truth for the wording, no second
  copy), in the BPR-1881 agent accent.
- Its actions work in place: retry the same request, or open the Agent tab's
  setup or settings when the fix is there. "Show the log" may still go to the
  Agent tab's run.
- It is dismissible, and it clears when the request is retried or succeeds.
- Do not also switch tabs automatically.

## Acceptance

- AC1. A failed Tour tab agent request shows its failure on the Tour tab,
  next to its control, without a tab switch. It uses the same wording the
  Agent tab shows for that failure.
- AC2. Retry from the Tour tab reruns the same request. A setup or settings
  action opens the right place.
- AC3. Successful requests show nothing new.
- AC4. Owner look: attended Accept/Reject on a frozen build that shows a
  failure on the Tour tab.
- AC5. `python3 scripts/verify_documentation.py --root .` passes.
