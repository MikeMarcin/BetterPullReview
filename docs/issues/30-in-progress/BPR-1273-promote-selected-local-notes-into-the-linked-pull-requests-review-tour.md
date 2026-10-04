---
schema: "implement-rfc.card.v1"
id: "BPR-1273"
title: "Promote selected local notes into the linked pull request's Review Tour"
status: "In Progress"
rfcs: ["RFC-BPR-0059", "RFC-BPR-0061"]
depends_on: ["BPR-1177"]
priority: "P3"
metadata: {"pack": "bpr-review-cycles-v1", "found_by": "BPR-1177", "related_issues": ["BPR-1177"], "acceptance": ["RC59-09"]}
---

# BPR-1273: Promote selected local notes into the linked pull request's Review Tour

## Why this card exists

RFC-BPR-0059's publication contract says: "Historical/off-hunk notes use the
existing exact placement/fallback policy when deliberately promoted, never
retargeted to convenient current lines." [[BPR-1177]] delivered Publish as
Pull Request, local continuity and the pull request handoff, and its build
offers no promotion at all: every local note stays in the cycle, the pull
request's description is the reviewer's Problem and Solution, and a Review
Tour is authored on the opened pull request through the existing tour
workflow, whose destination policy (`core::plan_tour_destination`) is
unchanged. So no local note is ever placed on a line by that build, and none
is retargeted; but nothing carries a chosen note across either.

## Outcome

From a linked cycle, the reviewer can choose notes and add them to the pull
request's Review Tour as stops, without rewriting them:

- A note attached to the published revision, whose file the new commit holds
  with the same bytes, keeps its exact lines as the stop's source at the
  pull request's head.
- Any other note (an earlier revision, a file the commit changed since, a
  folder or the whole cycle) carries no current source: the tour's existing
  policy decides its representation (file or conversation permalink) when the
  tour is published. Nothing chooses a convenient current line.
- Nothing is published by adding stops; the tour is published only through
  its own preview and confirmation.

## Acceptance and direct evidence

A note on the published lines, an off-hunk note, a historical note and a
whole-cycle note are added to the tour of the linked pull request; each
stop's source and planned destination are what the existing policy gives,
and the stops wait for the reviewer to complete them. Criteria: RC59-09's
promotion share.

## Ownership and constraints

Preserve the core/host/Qt allocation, the review tour store's ownership and
the side-effect boundary: fixtures only for GitHub, nothing published by
promotion.
