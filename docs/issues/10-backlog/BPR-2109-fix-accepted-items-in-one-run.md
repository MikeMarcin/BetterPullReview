---
schema: "implement-rfc.card.v1"
id: "BPR-2109"
title: "Fix accepted items in one run"
status: "Backlog"
rfcs: ["RFC-BPR-0123"]
depends_on: ["BPR-2108", "BPR-2068"]
metadata: {"criteria":["AC02","AC03","AC04","AC08"],"milestone_plan":{"id":"address-review-feedback-v1","path":"docs/rfcs/plans/address-review-feedback-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2105--BPR-2114--75fe99c5f481.json","imported_from":"docs/rfcs/assets/address-review-feedback-v1/cards/DRAFT-0005-fix-accepted-items-in-one-run.md"}
---

# Fix accepted items in one run

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0123](../../rfcs/RFC-BPR-0123-the-agent-evaluates-feedback-fixes-what-i-accept-and-drafts-the-replies.md), criteria `AC02`, `AC03`, `AC04`, `AC08`.

One change run over the items whose disposition is Fix, under RFC-BPR-0115's grant and fix-run rules; the per-item report; attribution of changed files to items shown as a claim, and the unattributed mark; the single item's Fix; and Check the fix per item and for all fixed items. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

A fixture agent that fixes several items, fails one, changes a file it attributes to none and attributes one file to two items, against real temporary worktrees. Keep and Revert keep RFC-BPR-0115's tests.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/address-review-feedback-v1/IMPLEMENTATION_PROMPT.md).
