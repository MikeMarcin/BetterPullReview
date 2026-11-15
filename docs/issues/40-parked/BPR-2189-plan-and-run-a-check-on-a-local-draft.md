---
schema: "implement-rfc.card.v1"
id: "BPR-2189"
title: "Plan and run a check on a local draft, and read its report"
status: "Parked"
rfcs: ["RFC-BPR-0120"]
depends_on: []
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"milestone_plan":{"id":"agent-assistance-v1","path":"docs/rfcs/plans/agent-assistance-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2189--BPR-2189--1aa8242b9237.json","replaces":"BPR-2082 (closed as rejected with the evidence design)","owner_decision":{"date":"2026-11-15","quote":"Rewrite without evidence"}}
parked_on: "human-review"
---

# BPR-2189: Plan and run a check on a local draft, and read its report

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0120](../../rfcs/RFC-BPR-0120-the-agent-runs-a-planned-check-on-a-local-pr-draft.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`.

Plan a check… in the local draft's whole-change conversation and its plan
card with Run and Discard; the check copy materialized from the candidate,
with RFC-BPR-0104's two grants remembered per draft and agent; the run with
its visible commands and Stop; the report card and the report tab with its
outcome word, commands, failure lines, code references and what was not
covered; Run again on a newer candidate; the built-in prompt
`local-draft.run-check`. Load `frontend-design:frontend-design` before the Qt
work and say so in the report.

## Parked: the owner reviews the rewrite first

The owner directed the rewrite on 2026-11-15 ("Rewrite without evidence") and
reviews the rewritten RFC, including its open questions Q1 to Q5, before any
build. This card does not start until that review is answered; an answer that
changes a default changes the RFC first.

## Intended verification

A fixture agent that plans, passes, fails, stops and changes a file in its
copy, against real temporary worktrees and check copies; the author's
worktree compared byte for byte before and after each run. The owner's look
(AC07) is an attended review on a frozen build with the fixture agent, one
question a candidate.
