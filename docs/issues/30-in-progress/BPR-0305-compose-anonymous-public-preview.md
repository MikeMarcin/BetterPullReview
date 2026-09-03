---
id: BPR-0305
title: Compose the production anonymous public preview
status: In Progress
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

## Lane — 2026-09-02

[[BPR-0303]], [[BPR-0304]], and [[BPR-0317]] are terminal; claimed straight
into `30-in-progress/` by the integrator, its worker started in a worktree
on the head that carries all three, under the transition exception. What
they leave at this card's door, all of it composed to `nullptr` today: the
host's `aAnonymousPullRequests` (a provider constructed with
`github_provider_read_authority::anonymous_public`), `aAnonymousSmartGit`
(a factory over `make_native_https_anonymous_smart_transport`), and the
`public_preview_projection_port` the window implements — plus
`set_public_preview_now`, which nothing binds yet, so without composition
the ring's reset text measures from zero. The ledger's selection table says
the preview is unavailable on every platform through [[BPR-0306]] and
enabled on macOS only by [[BPR-0316]], so this card composes every port and
keeps the shipped selection unavailable, proving the typed refusal with zero
requests and, under the tests-enabled fixture, the enabled course.
