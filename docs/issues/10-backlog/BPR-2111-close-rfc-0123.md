---
schema: "implement-rfc.card.v1"
id: "BPR-2111"
title: "Verify and close RFC-BPR-0123"
status: "Backlog"
rfcs: ["RFC-BPR-0123"]
depends_on: ["BPR-2108", "BPR-2109", "BPR-2110"]
closeout_for: ["RFC-BPR-0123"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07","AC08","AC09"],"milestone_plan":{"id":"address-review-feedback-v1","path":"docs/rfcs/plans/address-review-feedback-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2105--BPR-2114--75fe99c5f481.json","imported_from":"docs/rfcs/assets/address-review-feedback-v1/cards/DRAFT-0007-close-agent-feedback-work.md","draft_title":"Close the agent's feedback work"}
---

# BPR-2111: Verify and close RFC-BPR-0123

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0123](../../rfcs/RFC-BPR-0123-the-agent-evaluates-feedback-fixes-what-i-accept-and-drafts-the-replies.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`, `AC08`, `AC09`.

Acceptance assessment for evaluation, the batch fix and reply drafting.

## Intended verification

Judge every criterion on the composed application with an installed agent as well as the fixture agent.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/address-review-feedback-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `91cce126bf211865298f58c79ba86e09bc2b077a`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Evaluate feedback is a read-only run over the open or selected items, and each item then shows the agent's proposed disposition, reason and, for Fix, plan, marked as the agent's; no disposition changes until the author accepts or chooses one; an item with no returned verdict shows as not evaluated. | Pending | Pending | Not implemented |
| **AC02:** In every brief of the three operations the reviewers' text is carried as marked quoted data, and a conversation containing an instruction to the agent changes neither what is run nor what is granted. | Pending | Pending | Not implemented |
| **AC03:** Fix accepted items is one change run under the Edits this worktree grant over the items whose disposition is Fix; it makes no commit; and its changes are kept or reverted per file as RFC-BPR-0115 defines. | Pending | Pending | Not implemented |
| **AC04:** The run's changes show the items the agent attributes each file to, as a claim, and mark a changed file attributed to none; each item shows changed, or could not with the reason. | Pending | Pending | Not implemented |
| **AC05:** Draft replies writes one draft reply per item with a disposition and no reply, none for Needs your answer, as editable drafts on their conversations, and a review summary's reply as a pull request comment draft; nothing is posted. | Pending | Pending | Not implemented |
| **AC06:** A draft reply naming a commit that is not on the draft's branch is refused at publication with the commit named, and one naming a commit that is on it is not. | Pending | Pending | Not implemented |
| **AC07:** Evaluation and reply drafting each have their own built-in prompt; an edit to one reaches that operation's next brief and no other prompt. | Pending | Pending | Not implemented |
| **AC08:** None of the three operations starts without a press, resolves a conversation, posts to GitHub or marks an item Done. | Pending | Pending | Not implemented |
| **AC09:** On a frozen build with a fixture agent, the owner accepts the evaluated items, the batch fix's changes with their attribution, and the drafted replies. | Pending | Pending | Not implemented |
