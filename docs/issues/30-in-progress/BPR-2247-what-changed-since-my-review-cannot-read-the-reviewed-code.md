---
schema: "implement-rfc.card.v1"
id: "BPR-2247"
title: "What changed since my review says the reviewed code can't be read here"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner report, 2026-11-17","reservation":"docs/issues/_reservations/BPR-2247--BPR-2247--4e65a1072450.json","related":["BPR-2073","BPR-2167","BPR-2184","BPR-2246"],"acceptance":["BPR-2247:missing-earlier","BPR-2247:by-sha","BPR-2247:base-merges","BPR-2247:refusal"]}
---

# BPR-2247: What changed since my review says the reviewed code can't be read here

## Report

The owner, 2026-11-17: "I'm getting a lot of PRs I've reviewed before coming
back with new commits and getting "The code you reviewed can't be read here"
in my ask menu when I want to ask what changed. Seems like there are several
merge commits in the commits tab but there are a few real ones too and it
stinks that I can't do this"

Owner guidance, 2026-11-17: "We want if possible for things to just work."
Fetch what is missing and answer; refuse only when the reviewed code truly
cannot be got, and then say why.

## Scope

- What changed since my review is offered when the commit the reviewer
  reviewed is not among the review's own objects: it is fetched (by SHA when
  it is no longer within the head's history).
- Merges of the base branch since the review do not bring the base's changes
  into the change the agent is given.
- When the reviewed commit truly cannot be got, the refusal says so
  accurately.

## Acceptance

- `BPR-2247:missing-earlier`: a review whose objects do not hold the
  reviewed commit (a sparse review, which holds only its heads' commits and
  root trees) reads the change since the review.
- `BPR-2247:by-sha`: a reviewed commit outside the head's fetched history
  (beyond the depth, or force-pushed away) is fetched by its SHA.
- `BPR-2247:base-merges`: with merges of the base branch among the commits
  since the review, the change carries the real commits' own changes and
  none of the base's.
- `BPR-2247:refusal`: a reviewed commit GitHub no longer serves leaves the
  question unavailable with words that say the commit could not be fetched.
