---
schema: "implement-rfc.card.v1"
id: "BPR-2077"
title: "Verify and close RFC-BPR-0117"
status: "Backlog"
rfcs: ["RFC-BPR-0117"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0005-close-explain-this-file.md","draft_title":"Close Explain this file"}
depends_on: ["BPR-2076"]
closeout_for: ["RFC-BPR-0117"]
---

# BPR-2077: Verify and close RFC-BPR-0117

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0117](../../rfcs/RFC-BPR-0117-explain-this-file-where-i-am-reading.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`.

Acceptance assessment for Explain this file.

## Intended verification

Judge every criterion on the composed application.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `a9663a706fd72061f60b615bd8e749ee0ddb66b5`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Explain this file on a file's header and context menu sends `analyze_current_file` with that file as the subject and no selection, and the turn is in the pull request's Agent tab history. | Pending | Pending | Not implemented |
| **AC02:** The answer shows in a card at the file's header in Review with its references as links, and a running turn shows its activity there. | Pending | Pending | Not implemented |
| **AC03:** Keep as note makes a private file-level note marked with its agent origin; Dismiss removes the card and no note; Continue in Agent tab opens that turn. | Pending | Pending | Not implemented |
| **AC04:** The operation is briefed by its own built-in prompt, whose edit reaches the next Explain this file and no other prompt. | Pending | Pending | Not implemented |
| **AC05:** Opening, scrolling to or accepting a file launches no agent, and the operation writes nothing to GitHub. | Pending | Pending | Not implemented |
| **AC06:** On a frozen build with a fixture agent, the owner accepts the control and the answer card. | Pending | Pending | Not implemented |
