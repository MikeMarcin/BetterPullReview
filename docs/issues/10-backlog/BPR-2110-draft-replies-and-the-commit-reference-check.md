---
schema: "implement-rfc.card.v1"
id: "BPR-2110"
title: "Draft replies and the commit reference check"
status: "Backlog"
rfcs: ["RFC-BPR-0123"]
depends_on: ["BPR-2108"]
metadata: {"criteria":["AC02","AC05","AC06","AC07","AC08","AC09"],"milestone_plan":{"id":"address-review-feedback-v1","path":"docs/rfcs/plans/address-review-feedback-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2105--BPR-2114--75fe99c5f481.json","imported_from":"docs/rfcs/assets/address-review-feedback-v1/cards/DRAFT-0006-draft-replies-and-the-commit-reference-check.md"}
---

# Draft replies and the commit reference check

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0123](../../rfcs/RFC-BPR-0123-the-agent-evaluates-feedback-fixes-what-i-accept-and-drafts-the-replies.md), criteria `AC02`, `AC05`, `AC06`, `AC07`, `AC08`, `AC09`.

The read-only run that drafts one reply per item with a disposition and no reply, as editable drafts on their conversations and as a pull request comment draft for a review summary; core's check that a commit a draft names is on the draft's branch; and the built-in prompt `local-draft.feedback-reply`. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Core tests for the commit check with a commit on the branch, on another branch and unknown. A fixture agent drafting for each disposition. The owner's look (AC09) is an attended review on a frozen build with the fixture agent, covering evaluation, the batch fix and the drafts.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/address-review-feedback-v1/IMPLEMENTATION_PROMPT.md).
