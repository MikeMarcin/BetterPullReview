---
schema: "implement-rfc.card.v1"
id: "BPR-2033"
title: "Provide a resumable companion action contract"
status: "Backlog"
rfcs: ["RFC-BPR-0112"]
metadata: {"criteria":["AC03","AC04","AC07","AC09"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0018","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0045-provide-a-resumable-companion-action-contract.md"}
depends_on: ["BPR-2025", "BPR-2028", "BPR-2032"]
---

# Provide a resumable companion action contract

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0112](../../rfcs/RFC-BPR-0112-optional-workflow-extensions.md), criteria `AC03`, `AC04`, `AC07`, `AC09`.

Bounded context/proposal/check/input/result interface and synthetic organization companion; upstream compatibility observations stay outside core.

The RFC's section "Fitted to this pack's other decisions" is part of this card's scope.

## Intended verification

Pause and resume local preparation after author input or changed upstream rules without copying project policy, forging markers, creating a PR or using a publishing command as dry-run.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).
