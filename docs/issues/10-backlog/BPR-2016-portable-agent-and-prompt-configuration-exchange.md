---
schema: "implement-rfc.card.v1"
id: "BPR-2016"
title: "Portable agent and prompt configuration exchange"
status: "Backlog"
rfcs: ["RFC-BPR-0107"]
metadata: {"criteria":["AC02","AC03","AC04","AC05","AC07"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0001","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0028-portable-agent-and-prompt-configuration-exchange.md"}
depends_on: []
---

# Portable agent and prompt configuration exchange

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0107](../../rfcs/RFC-BPR-0107-agent-and-prompt-sharing.md), criteria `AC02`, `AC03`, `AC04`, `AC05`, `AC07`.

Core exchange admission and import mapping; application file/persistence coordination; narrow settings-store integration. Preserve current agent and prompt validators and the existing rule for when an edited entry keeps its identity.

The RFC's section "Fitted to this pack's other decisions" is part of this card's scope.

## Intended verification

Round-trip selected portable definitions and refuse malformed, incompatible or conflicting input without effects or partially applied settings. Replace an entry with and without a change to what it runs, and show which keeps its retained conversations; import a definition requesting relaxed checks without accepting the relaxation.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).
