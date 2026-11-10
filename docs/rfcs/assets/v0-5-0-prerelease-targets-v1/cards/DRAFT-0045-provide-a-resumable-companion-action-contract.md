---
schema: "implement-rfc.card.v1"
id: "DRAFT-0045"
title: "Provide a resumable companion action contract"
status: "Backlog"
rfcs: ["RFC-DRAFT-0015"]
depends_on: ["DRAFT-0037", "DRAFT-0040", "DRAFT-0044"]
metadata: {"criteria": ["AC03", "AC04", "AC07", "AC09"], "milestone_plan": {"id": "v0-5-0-prerelease-targets-v1", "path": "docs/rfcs/assets/v0-5-0-prerelease-targets-v1/milestone-plan.json"}, "absorbed_from": "local PR drafts pack v1, DRAFT-0018"}
---

# Provide a resumable companion action contract

## Scope and acceptance

Authoritative requirements: [RFC-DRAFT-0015](../RFC-DRAFT-0015-optional-workflow-extensions.md), criteria `AC03`, `AC04`, `AC07`, `AC09`.

Bounded context/proposal/check/input/result interface and synthetic organization companion; upstream compatibility observations stay outside core.

The RFC's section "Fitted to this pack's other decisions" is part of this card's scope.

## Intended verification

Pause and resume local preparation after author input or changed upstream rules without copying project policy, forging markers, creating a PR or using a publishing command as dry-run.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../IMPLEMENTATION_PROMPT.md).
