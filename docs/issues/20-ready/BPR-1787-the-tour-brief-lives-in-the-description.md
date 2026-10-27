---
schema: "implement-rfc.card.v1"
id: "BPR-1787"
title: "The tour's Problem and Solution live in the pull request description, not also in the tour comment"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1787--BPR-1787--ec08212cbca8.json","kind":"feature","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0040","RFC-BPR-0042"]}
acceptance: []
---

# BPR-1787: The tour brief lives in the pull request description

## Request

The owner, 2026-10-27, verbatim:

> It's weird to have the problem and solution in both the review tour and the
> pr description body. I feel like it should be in one or the other and if
> bias towards the description body.

## Grounded current behavior

- `src/app/src/tour_publication_plan.cpp` always sets the tour comment's
  body (the review, or the compatibility index comment) to
  `## Problem … ## Solution …`, followed by the route.
- **Update PR description after publishing** (`tourDescriptionEdit`) is off
  by default. When on, `core::propose_tour_description` maps or inserts the
  same Problem and Solution into the description, and BPR-1786 puts the
  `[Review tour](…)` link at its top. Both places then carry the brief.
- Import (`review_tour_import.cpp`), reconciliation
  (`review_tour_reconciliation.cpp`, `retain_tour_brief_baselines`) and
  update (`review_tour_update.cpp`) read and patch the brief wherever it was
  published (`ProblemBaseline` / `SolutionBaseline` record the entity).

## Outcome

- The brief is published in exactly one place. By default it is the
  description: **Update PR description** starts on, and when it is on the
  tour comment carries only the route (its "Review tour — Start here" list).
  It does not repeat Problem and Solution. A short line pointing to the
  description is allowed if the comment would otherwise read oddly.
- An author who turns the description update off gets today's behavior: the
  brief is in the tour comment and the description is untouched.
- Import, reconciliation and update find the brief where it was published:
  in the description for new tours, and in the comment for tours published
  before this change, which keep working and are not rewritten unasked.
- The preview says where the brief will go.

## Acceptance (draft)

- Core and application tests: a plan with the description on has a tour body
  without Problem/Solution and a description with them; with it off, the
  reverse. An old tour (brief in the comment) still imports, reconciles and
  updates. A new tour (brief in the description) imports from a cold profile
  with its Problem and Solution.
- The publication fixture courses (native, compat, reconciliation, manual,
  cold import) pass with the new default and check where the brief went.
- The owner's look at the preview wording (attended review).
