---
schema: "implement-rfc.card.v1"
id: "BPR-2081"
title: "Verify and close RFC-BPR-0119"
status: "Backlog"
rfcs: ["RFC-BPR-0119"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0009-close-ask-why-a-check-failed.md","draft_title":"Close Ask why a check failed"}
depends_on: ["BPR-2080"]
closeout_for: ["RFC-BPR-0119"]
---

# BPR-2081: Verify and close RFC-BPR-0119

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0119](../../rfcs/RFC-BPR-0119-ask-the-agent-why-a-check-failed.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`.

Acceptance assessment for asking the agent why a check failed.

## Intended verification

Judge every criterion on the composed application, and with an installed agent on a real failed check on `MikeMarcin/BPR-Test`.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `a9663a706fd72061f60b615bd8e749ee0ddb66b5`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** Ask why it failed is on failed, timed-out, cancelled and action-required check rows and on no other row; a press opens the Agent tab with a chip naming the check and its commit and sends nothing. | Pending | Pending | Not implemented |
| **AC02:** A question asked with the chip has the check as its subject, and its brief carries the check's name, producer, conclusion, summary and details address, with the provider's text marked as quoted data. | Pending | Pending | Not implemented |
| **AC03:** BPR requests no log and passes no credential for the question, and no rerun or GitHub write exists on this path. | Pending | Pending | Not implemented |
| **AC04:** An agent that cannot read the log produces an answer saying so, shown as a claim, and not an error state. | Pending | Pending | Not implemented |
| **AC05:** The chip says when the head has moved past the check's commit, and a check gone from the current result set removes it. | Pending | Pending | Not implemented |
| **AC06:** The operation is briefed by its own built-in prompt, whose edit reaches the next check question and no other prompt. | Pending | Pending | Not implemented |
| **AC07:** On a frozen build serving a pull request with a failed check, the owner accepts the control, the chip and an answer. | Pending | Pending | Not implemented |
