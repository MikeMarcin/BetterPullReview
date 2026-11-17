---
schema: "implement-rfc.card.v1"
id: "BPR-2251"
title: "What changed since my review reads the reviewed code only when asked"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner, 2026-11-17","reservation":"docs/issues/_reservations/BPR-2251--BPR-2251--b7b70de4ba63.json","related":["BPR-2247","BPR-2248"],"acceptance":["BPR-2251:no-prefetch","BPR-2251:offered","BPR-2251:on-press","BPR-2251:refusal"]}
---

# BPR-2251: What changed since my review reads the reviewed code only when asked

## Report

The owner, 2026-11-17: "Ideally fetching those historical things doesn't
slow down our loading of the current state for reviewing". Approach chosen
by the owner: "Put it off until you need it is probably the right lazy
approach".

[[BPR-2247]] made What changed since my review fetch the reviewed commit,
its history and file contents when the review lacks them. The window read
that eagerly as soon as a returning pull request loaded, on the review's
serial cache actor.

## Scope

- Opening a pull request reads nothing of its past for What changed since
  my review.
- The question is offered whenever an earlier reviewed (or Seen) head
  differs from the current head.
- A press reads the change since the review, fetching what the review lacks
  as [[BPR-2248]]'s commit asks do, then asks.
- A fetch that fails is refused on the press with words that say why, not
  as a disabled menu item.

## Acceptance

- `BPR-2251:no-prefetch`: a returning pull request's current state loads and
  its files' diffs read while no historical read is issued; a test with a
  blocked historical fetch proves the load does not wait on it.
- `BPR-2251:offered`: the Pull Request menu and the Agent tab offer the
  question as soon as the earlier head differs from the head.
- `BPR-2251:on-press`: a press reads the change and sends it with the
  question.
- `BPR-2251:refusal`: a reviewed commit GitHub no longer serves refuses the
  press, saying it could not be fetched; nothing is sent.
