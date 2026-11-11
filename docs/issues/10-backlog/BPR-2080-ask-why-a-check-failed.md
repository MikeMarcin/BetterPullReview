---
schema: "implement-rfc.card.v1"
id: "BPR-2080"
title: "Ask why a check failed"
status: "Backlog"
rfcs: ["RFC-BPR-0119"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2073--BPR-2085--88e53295c309.json","imported_from":"docs/rfcs/assets/agent-assistance-v1/cards/DRAFT-0008-ask-why-a-check-failed.md"}
depends_on: []
---

# Ask why a check failed

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0119](../../rfcs/RFC-BPR-0119-ask-the-agent-why-a-check-failed.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`.

The control on failing check rows; the chip and its prompts; the check as a fifth question subject; the brief built from what BPR already holds, with provider text as quoted data; the stale and removed states; and its own built-in prompt, `review.check-failure`. BPR downloads no log and passes no credential. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Core tests for which rows offer the control and for the new subject. The request log of a fixture session shows no log download. A fixture agent with and without access to the log. The owner's look (AC07) is an attended review on a frozen build serving a pull request with a failed check.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/agent-assistance-v1/IMPLEMENTATION_PROMPT.md).
