---
schema: "implement-rfc.card.v1"
id: "BPR-2079"
title: "Verify and close RFC-BPR-0118"
status: "Backlog"
rfcs: ["RFC-BPR-0118"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0007-close-suggest-the-change.md","draft_title":"Close Suggest the change"}
depends_on: ["BPR-2078"]
closeout_for: ["RFC-BPR-0118"]
---

# BPR-2079: Verify and close RFC-BPR-0118

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0118](../../rfcs/RFC-BPR-0118-turn-a-draft-comment-into-an-exact-suggested-change.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`.

Acceptance assessment for turning a draft comment into a suggested change.

## Intended verification

Judge every criterion on the composed application with an installed agent as well as the fixture agent.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `a9663a706fd72061f60b615bd8e749ee0ddb66b5`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Suggest the change is offered only on a draft comment whose lines can take a suggestion under RFC-BPR-0090's range rules, and a press sends the comment workshop entry point with the new operation and exactly those lines. | Pending | Pending | Not implemented |
| **AC02:** A reply whose stated current text equals the head's lines opens as an in-pane suggestion edit showing the agent's replacement, and the draft comment is unchanged until the reviewer keeps it. | Pending | Pending | Not implemented |
| **AC03:** A reply whose range differs, whose stated current text differs by any byte, whose replacement is identical or out of bounds, or which GitHub could not take as a suggestion is refused with that reason and never shown as a suggestion; a head that moved the lines during the run is such a refusal. | Pending | Pending | Not implemented |
| **AC04:** Keeping the suggestion adds the suggestion block to the draft and leaves its text as written; one Undo restores the draft; nothing is sent to GitHub. | Pending | Pending | Not implemented |
| **AC05:** The operation is briefed by its own built-in prompt, whose edit reaches the next Suggest the change and no other prompt. | Pending | Pending | Not implemented |
| **AC06:** On a frozen build with a fixture agent, the owner accepts the control, an applied suggestion and a refused one. | Pending | Pending | Not implemented |
