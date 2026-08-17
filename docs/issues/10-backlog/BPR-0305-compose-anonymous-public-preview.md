---
id: BPR-0305
title: Compose the production anonymous public preview
status: Backlog
rfc: RFC-BPR-0017
depends_on: [BPR-0303, BPR-0304, BPR-0317]
---

# Compose the production anonymous public preview

## Required final state

- The tests-disabled application composes the existing provider/normalizer and
  Git/cache/epoch/diff paths behind explicit platform selection, while
  production macOS, Linux, and Windows remain typed public-preview unavailable
  with zero requests in this candidate slice.
- BPR-0316 owns same-slice macOS enablement and selected-artifact rerun after
  BPR-0306 passes. Linux and Windows remain unavailable with zero
  requests; enabling either requires a separately allocated target-native card
  and an update to BPR-0309 before closeout.
- GHES, mutation, GraphQL, viewer, vault, credential, and private-intent paths
  refuse before request construction; counters prove zero effects.
- The composed course opens a recorded public fixture, renders flat/grouped
  comments without resolution, selects a file and renders its exact diff
  content, then switches to authenticated mode with exact invalidation and no
  stale identity-sensitive projection.
- The designated public-observer subprocess and all fixture helpers remain
  outside production selection and testing symbols are absent from Release.
