---
schema: "implement-rfc.card.v1"
id: "BPR-1273"
title: "Promote selected local notes into the linked pull request's Review Tour"
status: "Done"
rfcs: ["RFC-BPR-0059", "RFC-BPR-0061"]
depends_on: ["BPR-1177"]
priority: "P3"
metadata: {"pack": "bpr-review-cycles-v1", "found_by": "BPR-1177", "related_issues": ["BPR-1177"], "acceptance": ["RC59-09"]}
acceptance: [{"criterion":"RC59-09","outcome":"pass","revision":"44e7b8a6ce2b39ad358f1a4b4efbe55c4a58c2bf","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1273's promotion share. core_review_cycle_promotion; review_cycle_publication_matrix (28 cases, six promotion cases with kills before and after durable stops and a lost answer); application_review_cycle_publish_public_path (a line note added through the desktop's tour owner to the tour of #17 with its exact line at the published commit, again without a second stop, nothing sent to GitHub); 75 of 75 integration rows; six negative controls. A chosen note joins the linked pull request's Review Tour through the tour's own store, receipts and owner; its source and planned destination are the existing placement policy's; historical and off-commit notes carry no source; stops wait for the reviewer; no private history uploaded and no review event or comment written. macOS arm64; GitHub simulated."}}]
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

## Implemented — macOS, 2026-10-04

At `44e7b8a6c`. Share notes' menu holds **Add Notes to Review Tour…** for a
linked cycle, a sheet over the code. It lists every note in two lines: where
it is and its first words, then where its stop would point and what the
tour's own policy makes of it on GitHub, with a ledger saying nothing goes to
GitHub now and what stays on this computer. A chosen note becomes one stop
with its current wording, unchanged. A note on the published lines, or on a
line of a published file the change did not touch, keeps its exact lines at
the published commit, proved by the plan and the note's bytes; every other
note (an earlier capture, other bytes in the commit, a file outside its
changes, a whole file, a folder, the whole change, a pull request linked by
hand) carries no source, and the reviewer chooses its lines when completing
it. Stops wait in the editing state. Adding runs where the tour's draft lives,
through the profile's tour owner, under a request identity of the cycle, the
note and the pull request; the tour's receipts are read first, so a lost
answer or a killed process never adds a note twice. Nothing is sent to
GitHub; the tour is published only through its own preview.

Evidence, the six promotion cases added to the publication matrix, six
negative controls and the limits are in the [execution
ledger](../../verification/RFC-BPR-0053-0061.md), under "BPR-1273: local
notes promoted into the linked pull request's Review Tour". No criterion
needed the owner; no attended session was opened.

RC59-09 passes for this card's share; with BPR-1177's, RFC-BPR-0059's RC59-09
row has its evidence for closeout ([[BPR-1192]]).

**Note: Linux run at `f389470f2` (station `bpr-main-linux`, WSL2, `build/dev`,
GCC 15.2, offscreen).** The build succeeded and 16 of 16 passed:
`core_review_cycle_promotion`, `review_cycle_publication_matrix` (26.4 s),
`application_review_cycle_publish_public_path`,
`application_review_round_trip_composition_contract`,
`qt_review_cycle_view_1_0` and `_2_0`, the nine
`application_review_tour_publication_*` rows and `architecture_boundaries`.
GCC compiled `tests/core/review_cycle_promotion_tests.cpp` under `-Werror`
with no diagnostic at line 179 or elsewhere, so the possibly ignored
`[[nodiscard]]` result Clang flagged is not rejected here and nothing changed.
Done is not reopened.
