---
schema: "implement-rfc.card.v1"
id: "BPR-2019"
title: "Discover the worktree and prospective PR comparison"
status: "Done"
rfcs: ["RFC-BPR-0108"]
metadata: {"criteria":["AC01","AC03"],"milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"absorbed_from":"local PR drafts pack v1, DRAFT-0004","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0031-discover-the-worktree-and-prospective-pr-comparison.md"}
depends_on: []
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"3585dedc7f50ec7296cd345a44b2fa1fe87378cc","evidence":{"kind":"command","detail":"Real primary clone, linked worktree and shared isolated clone open with source, hosted target, merge base apart from the tip, only the branch's changes and an unchanged checkout; the desktop course reaches the diff from Prepare Pull Request signed out; negative control (tip as base) fails","command":"ctest --preset dev -R '^(local_draft|application_local_draft_desktop_public_path)$'"}},{"criterion":"AC03","outcome":"pass","revision":"3585dedc7f50ec7296cd345a44b2fa1fe87378cc","evidence":{"kind":"command","detail":"Chain to the hosted remote without fetching, borrowed objects, cycle, fork, no common base, missing tip, unknown default branch and detached head each a specific recovery with existing drafts intact","command":"ctest --preset dev -R '^(core_local_draft|local_draft)$'"}}]
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

## Implementation record

Commits `fe3a4a2e1` (core), `c56f0b638` (Git reader, discovery, store, host),
`1698f5b67` (real-worktree course), product candidate `3585dedc7`. The
installed-Git local-draft reader runs `merge-base --all`, `ls-tree` and
`cat-file` in the publication catalog's isolated store with the worktree's
and the remote chain's objects as alternates; refs, config and the index are
read as data through the workspace port. Results, the negative control and
limits: [ledger](../../verification/RFC-BPR-0108.md).
