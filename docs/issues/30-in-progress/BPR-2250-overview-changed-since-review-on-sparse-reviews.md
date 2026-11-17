---
schema: "implement-rfc.card.v1"
id: "BPR-2250"
title: "The Overview's changed-since-my-review mark is never shown on a sparse review"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","reservation":"docs/issues/_reservations/BPR-2250--BPR-2250--c51365348293.json","origin":"BPR-2247 remaining risk, 2026-11-17","related":["BPR-0654","BPR-2247"]}
---

# BPR-2250: The Overview's changed-since-my-review mark is never shown on a sparse review

## Finding

[[BPR-2247]] found that most pull requests load sparsely
(`object_loading_for` in `src/application/src/github_window_read_host.cpp`
answers `selected_objects` within the read limits), so the review's cache holds
only its heads' commits and root trees. What changed since my review now reads
through `resolve_returning_change`, which fetches what the cache lacks.

The Overview's "changed since my last review" mark ([[BPR-0654]]) still reads
through `resolve_changed_paths` (`changed_since_review` in
`src/app/src/github_application/window_read_state.cpp`). That read asks the
cache alone and never fetches: the reviewed head's commit and subtrees are
missing on a sparse review, so the read fails, the failure is not asked again
until an input moves, and the mark stays unshown. A force-pushed reviewed head
is never readable there either.

## Scope

- The mark reads the reviewed head through the review's own GitHub access
  when the cache lacks it, as `resolve_returning_change` does (trees only; by
  SHA when it is outside the head's history).
- When the reviewed commit truly cannot be got, the Overview shows no mark
  and nothing claims the files are unchanged.

## Acceptance

- `BPR-2250:sparse`: on a sparse review whose reviewed head is not among the
  review's objects, the Overview marks exactly the files that changed since
  the review.
- `BPR-2250:gone`: with a reviewed head GitHub no longer serves, no file is
  marked changed or unchanged, and nothing is asked again in a loop.
