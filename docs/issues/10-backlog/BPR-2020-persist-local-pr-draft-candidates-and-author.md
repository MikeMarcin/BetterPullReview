---
schema: "implement-rfc.card.v1"
id: "BPR-2020"
title: "Persist local PR draft candidates and author notes"
status: "Backlog"
rfcs: ["RFC-BPR-0108"]
metadata: {"criteria":["AC02","AC04","AC07"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0005","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0032-persist-local-pr-draft-candidates-and-author.md"}
depends_on: ["BPR-2019"]
---

# Persist local PR draft candidates and author notes

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0108](../../rfcs/RFC-BPR-0108-local-pr-drafts.md), criteria `AC02`, `AC04`, `AC07`.

Core local subject and candidate transitions; application capture/lifecycle and profile-private persistence, with shared material services; discard, bounded retention and coverage by the existing profile resets.

## Intended verification

Demonstrate dirty selection, restart, concurrent writer/late completion, branch switch and rebind without retargeting or partial ready material. Discard a draft and run a profile reset with the worktree unchanged; fill the store and observe the refusal.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).
