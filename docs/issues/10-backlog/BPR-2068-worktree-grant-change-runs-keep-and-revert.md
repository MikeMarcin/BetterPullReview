---
schema: "implement-rfc.card.v1"
id: "BPR-2068"
title: "The worktree grant, change runs, and Keep and Revert"
status: "Backlog"
rfcs: ["RFC-BPR-0115"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC11"],"milestone_plan":{"id":"local-draft-authoring-v1","path":"docs/rfcs/plans/local-draft-authoring-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2062--BPR-2071--3110069c9cbb.json","imported_from":"docs/rfcs/assets/local-draft-authoring-v1/cards/DRAFT-0007-worktree-grant-change-runs-keep-and-revert.md"}
depends_on: ["BPR-2062", "BPR-2007", "BPR-2026"]
---

# The worktree grant, change runs, and Keep and Revert

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0115](../../rfcs/RFC-BPR-0115-agent-changes-in-a-local-pr-draft-worktree.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC11`.

The change-run kind and its Changes session bound to the worktree; the Edits this worktree grant, its question, memory and withdrawal; Ask for a change…; the recorded state before a run; the run's changes with Keep and Revert through the write port; the report of a moved head or other changed branch or tag; and the exclusions while a run is active. Reconciles RFC-BPR-0104's live text under the pack's amendment.

## Intended verification

A fixture agent that edits, creates and deletes files, one that moves the head and one that moves another branch, against real temporary worktrees. Launch-argument tests show the grant passed for a local draft and never for a pull request review. Tour runs and one-shot hooks keep their existing read-only tests.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/local-draft-authoring-v1/IMPLEMENTATION_PROMPT.md).
