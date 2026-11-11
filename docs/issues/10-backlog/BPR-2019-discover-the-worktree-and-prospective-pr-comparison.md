---
schema: "implement-rfc.card.v1"
id: "BPR-2019"
title: "Discover the worktree and prospective PR comparison"
status: "Backlog"
rfcs: ["RFC-BPR-0108"]
metadata: {"criteria":["AC01","AC03"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0004","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0031-discover-the-worktree-and-prospective-pr-comparison.md"}
depends_on: []
---

# Discover the worktree and prospective PR comparison

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0108](../../rfcs/RFC-BPR-0108-local-pr-drafts.md), criteria `AC01`, `AC03`.

Narrow Git observations, local-clone remote resolution, target/base distinction and exact proposed comparison; no filesystem-adapter Git effects.

## Intended verification

Exercise a real linked-worktree entry, an isolated local clone of a primary clone (including `clone --shared` alternates), fork/target ambiguity and missing-base path through the composed author entry.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/v0-5-0-prerelease-targets-v1/IMPLEMENTATION_PROMPT.md).
