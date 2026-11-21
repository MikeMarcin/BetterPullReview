---
schema: "implement-rfc.card.v1"
id: "BPR-2328"
title: "List a conflicting pull request's files from a local merge check when the cache holds both sides"
status: "Backlog"
rfcs: []
depends_on: ["BPR-2327"]
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","target":"0.5.0","origin":"BPR-2327's investigation, 2026-11-20","reservation":"docs/issues/_reservations/BPR-2327--BPR-2328--f3b2d54d1f10.json","related_cards":["BPR-2327","BPR-1873"],"acceptance":["AC1","AC2","AC3"]}
---

# BPR-2328: List a conflicting pull request's files from a local merge check when the cache holds both sides

## Why

[[BPR-2327]] gave the Overview a place for the conflicting files
(`overview_context::ConflictingPaths`), but nothing supplies it. GitHub's API
does not report which files conflict. The review's Git cache could: `git
merge-tree --write-tree --name-only --no-messages <base> <head>` names them
without a working tree. It needs the base tip's, the merge base's and the
head's trees and blobs. A complete-closure review may hold them; a sparse
review (`git_revision_residency::sparse_commit_trees`, [[BPR-1873]]) holds no
blobs and cannot answer without a fetch.

## Acceptance

- AC1: for a conflicting pull request whose cache holds both sides, the
  application host runs the merge check once per accepted head and base as a
  cancellable bounded effect, writing no object into the shared cache (a
  private object directory borrowing the cache's), and supplies the paths.
- AC2: a sparse review, missing objects, or a failed or cancelled check
  supplies nothing, and the Overview keeps BPR-2327's "not reported" line. No
  fetch is started for it.
- AC3: fixture courses cover a cache with both sides, a sparse cache and a
  cancelled check.
