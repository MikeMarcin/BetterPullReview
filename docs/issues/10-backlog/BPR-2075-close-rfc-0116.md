---
schema: "implement-rfc.card.v1"
id: "BPR-2075"
title: "Verify and close RFC-BPR-0116"
status: "Backlog"
rfcs: ["RFC-BPR-0116"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0003-close-returning-pull-request.md","draft_title":"Close the returning pull request questions"}
depends_on: ["BPR-2073", "BPR-2074"]
closeout_for: ["RFC-BPR-0116"]
---

# BPR-2075: Verify and close RFC-BPR-0116

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0116](../../rfcs/RFC-BPR-0116-a-returning-pull-request-says-what-changed-and-whether-a-comment-was-addressed.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`.

Acceptance assessment for What changed since my review and Was this addressed?.

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
| **AC01:** What changed since my review is offered only when the pull request has commits newer than the reviewer's last review or Seen; a press sends `analyze_latest_upload` with both revisions named in the brief; and the reply's code references open as links. | Pending | Pending | Not implemented |
| **AC02:** When the earlier revision's material cannot be read, the control is unavailable with that reason, and no whole-pull-request answer is labelled as what changed. | Pending | Pending | Not implemented |
| **AC03:** Was this addressed? on a thread sends `evaluate_proposed_fix` with the conversation as its subject, with no selection required, and its brief carries the attached lines as they were and as they are. | Pending | Pending | Not implemented |
| **AC04:** The answer leads with addressed, partly, not addressed or cannot tell and is shown as the agent's claim; the thread is not resolved, no defect's disposition changes and nothing is posted. | Pending | Pending | Not implemented |
| **AC05:** Each operation is briefed by its own built-in prompt with its own editor, default, version and Reset; an edit reaches that operation's next brief and no other prompt. | Pending | Pending | Not implemented |
| **AC06:** Opening, refreshing and returning to a pull request with new commits launches no agent, and neither operation writes to GitHub or changes attention state. | Pending | Pending | Not implemented |
| **AC07:** On a frozen build serving a pull request with commits newer than a review and a commented thread, the owner accepts both controls and their answers. | Pending | Pending | Not implemented |
