---
schema: "implement-rfc.card.v1"
id: "DRAFT-0012"
title: "The Agents section in Insights"
status: "Backlog"
rfcs: ["RFC-DRAFT-0006"]
depends_on: []
metadata: {"criteria": ["AC01", "AC02", "AC03", "AC04", "AC05", "AC06"], "milestone_plan": {"id": "agent-assistance-v1", "path": "docs/rfcs/assets/agent-assistance-v1/milestone-plan.json"}}
---

# The Agents section in Insights

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0006](../RFC-DRAFT-0006-insights-shows-what-the-agents-did-and-what-was-kept.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`.

The closed list of agent features in core; counting runs, endings, reported spend and suggestion outcomes at the event; the store, purge, reset and export rules of RFC-BPR-0086; and the Agents section. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Core and store tests in the shape of the existing review-stat ones, including a missing cost and a kept-then-removed item. A test that fails when an agent feature has no row. The owner's look (AC06) is an attended review on a frozen build with seeded counts.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).
