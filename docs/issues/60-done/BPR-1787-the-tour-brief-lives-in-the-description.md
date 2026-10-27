---
schema: "implement-rfc.card.v1"
id: "BPR-1787"
title: "The tour's Problem and Solution live in the pull request description, not also in the tour comment"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1787--BPR-1787--ec08212cbca8.json","kind":"feature","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0040","RFC-BPR-0042"]}
acceptance: [{"criterion": "AC1 core decisions, both settings", "outcome": "pass", "revision": "f60d72b0b735a3323cbf9fb729cb413dd288a7fb", "evidence": {"kind": "command", "command": "ctest -R '^core_review_tour'", "detail": "core_review_tour_update brief_in_one_place: tour_comment_lead with the brief in the description is the one pointing line and carries no Problem/Solution; with it in the comment it is ## Problem/## Solution. A new tour (route-only comment, description baselines) has tour_brief_in_description and not tour_brief_in_comment; a brief change leaves the route comment with zero edits and maps into the description in place under the leading link; a website edit to the description reconciles. A cold-imported (exact-span) description proposes no edit while unchanged and keeps the section's blank lines when revised. An old tour (brief in its comment) has the reverse flags; its brief change is one edit to the comment; a website edit to the comment reconciles. A brief observed in another place than it was published is not merged (RemoteStale)."}}, {"criterion": "AC2 import, old and new tours", "outcome": "pass", "revision": "f60d72b0b735a3323cbf9fb729cb413dd288a7fb", "evidence": {"kind": "command", "command": "ctest -R '^core_review_tour_import$'", "detail": "A route-only root with the brief in the description (under its Review tour link) imports with Problem and Solution from the description, baselines naming the pull request, no mapping notice, and the linked root preferred. A pre-BPR-1787 root with ## Problem/## Solution and an untouched description imports both from the comment with the comment's entity as baseline. A brief in neither place still reports Problem requires source mapping."}}, {"criterion": "AC3 application: description-only update", "outcome": "pass", "revision": "f60d72b0b735a3323cbf9fb729cb413dd288a7fb", "evidence": {"kind": "command", "command": "ctest -R '^staged_review_session$'", "detail": "test_description_only_update: a new tour with the brief in the description publishes in 5 writes, the review summary starts with the pointing line and has no ## Problem, the description starts with the Review tour link and holds the brief; published_tour records the brief as in the description; revising Problem yields a plan with no comment edits whose only step is update_description; publishing it is one write, Published and DescriptionDone, the summary unchanged and the description equal to the proposal."}}, {"criterion": "AC4 publication fixture courses with the new default", "outcome": "pass", "revision": "f60d72b0b735a3323cbf9fb729cb413dd288a7fb", "evidence": {"kind": "command", "command": "ctest -R 'application_review_tour|qt_review_tour|staged_review_session|core_review_tour|application_review_round_trip_composition_contract'", "detail": "27/27 pass. Publication courses (native, compat, stop, description, conflict, unsupported, maintenance, reconciliation, manual) assert the checkbox starts on and its tooltip says where the brief goes. Native/compat/maintenance/reconciliation/manual (default on): preview says Problem and Solution go in the PR description and shows the pointing line; the review summary or compatibility index comment starts with the pointing line and the route and has no Problem/Solution; the description starts with the tour link, keeps the CRLF template and holds ## Problem/## Solution. Stop course (turned off): preview says the brief goes in the tour comment and the description will not be edited; the resumed review summary carries ## Problem/## Solution before the route; the description is byte-for-byte the original template; 4 writes. Reconciliation's cold-profile revision goes to the description only (6 writes, was 7) with the review summary unchanged, and the update preview says the brief stays in the PR description. Cold import course: the route-only fixture root's brief is recovered from the description. Composition, writing and CLI ordinary courses pass; application_composition_source_contract passes and the round-trip manifest pin needed no reseal (tour sources are outside it)."}}, {"criterion": "AC5 owner look at the preview wording", "outcome": "pending", "revision": "f60d72b0b735a3323cbf9fb729cb413dd288a7fb", "evidence": {"kind": "observation", "detail": "Deferred by the coordinator; no attended-review session was opened. Wording to judge: the checkbox tooltip, the preview lines 'Problem and Solution go in the PR description, not the tour comment.' / 'go in the tour comment.' / 'stay in ... where they were published.', and the comment's pointing line 'The problem and solution are in the pull request description.'"}}]
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

## Built

- Core (`review_tour_reconciliation`): `tour_comment_lead` decides what the
  tour comment opens with (`tour_brief_in_description_note`, the one line
  `The problem and solution are in the pull request description.`, or
  `## Problem … ## Solution …`); `tour_brief_in_comment` and
  `tour_brief_in_description` say where a published tour's brief lives (its
  index raw, and baselines naming an entity other than the tour root).
  Reconciliation compares a brief only with the place it came from.
  `propose_tour_description` keeps a cold-imported section verbatim until it
  changes, then keeps its own blank lines.
- Core (`review_tour_import`): the brief is read from the description first,
  then from the selected root's comment.
- Application (`staged_tour_update`): an update with no comment edits and a
  description edit is published once the description write is confirmed; the
  GitHub session's confirmation of an update's description write goes through
  `confirm_staged_update`.
- App (`tour_publication_plan`): a new tour puts the brief where the checkbox
  says; a published tour keeps it where it is and edits the description only
  when the brief lives there. The preview says where it goes. An unmatched
  description heading on a new tour names the way out (turn the option off).
- Qt: **Update PR description after publishing** starts on; its tooltip says
  where the brief goes either way. CLI: description by default,
  `--no-description` keeps the brief in the comment, both together refused.
- Docs: RFC-BPR-0040 published representation and import; `tour-cli.md`.

## Not verified

- The owner's look at the wording (AC5, pending, no session opened).
- No live run on BPR-Test.
- `test_tour_publication_cli` (tour-cli preset) was not built or run: the dev
  build does not configure it and disk was short. It passes `--description`
  explicitly, so the new default does not change its plan.
- `coding-tidy` was not run. `coding-format-check` fails only on
  `tests/github_contract/reaction_live_course.cpp`, which this card does not
  touch.
- The documentation gate stops on `docs/issues/NEXT_ID: next_id: BPR-1788
  rewinds historical high water BPR-1791`, a board counter state this card
  does not touch; it reported nothing about the changed documents.
- A new tour whose description write fails is published with its brief in
  neither place until Resume writes the description; refresh then reports the
  brief as needing mapping.
- Turning the checkbox on for a tour published before this change does not
  copy its brief into the description: published tours keep the brief where it
  was first published.
- Windows and Linux were not built.
