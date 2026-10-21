---
schema: "implement-rfc.card.v1"
id: "BPR-1688"
title: "Find in files and window restore under a commit filter"
status: "Backlog"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-1686"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1685--BPR-1688--a54310676f44.json","origin":"RFC-BPR-0089 'What a filter drives' and its window rule, which no milestone names","related_cards":["BPR-1686"]}
---

# BPR-1688: Find in files and window restore under a commit filter

RFC-BPR-0089 says a filter drives "Find in files, which says that it searches
the filtered set", and "The window keeps the filter with its comparison and
restores it with the window." Neither is in M1's milestone text (trees,
navigation, counts and the chip), nor in M2 to M4. M1 does neither.

## Today

- Find in files catalogues the comparison's own files and searches the merge
  base and head, also while a commit filter is active. It does not say so.
- A filter lives with the window's review. Reopening the window starts at
  Whole PR. `commit_filter_model::restore` exists for the host to call.

## Acceptance

- **AC1.** With a filter active, Find in files searches only the filtered
  files, each at the endpoints its filtered diff reads, and says that it
  searches the filtered set.
- **AC2.** The window keeps the filter with its comparison and restores it
  with the window; a restored filter that names a gone commit is shown as
  after a force push.
