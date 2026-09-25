---
schema: "implement-rfc.card.v1"
id: "BPR-1129"
title: "The title row and the review bar replace the composition entry row"
status: "In Progress"
rfcs: ["RFC-BPR-0052"]
depends_on: []
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner decision on BPR-1106, 2026-09-24","design":"docs/design/BPR-1106-scaffolding-v8.html","criteria":["AC-01","AC-02","AC-03","AC-10","AC-11"],"reservation":"docs/issues/_reservations/BPR-1129--BPR-1135--89a65c75ff1a.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
---

# BPR-1129: The title row and the review bar replace the composition entry row

## Outcome

The title row keeps Home, the drawer toggles, the pull request's name, the
modality at its right, refresh, the comments toggle with the new-comment
count, account and settings. A full-width review bar directly under the title row
takes what leaves it and replaces the composition entry row:

- from its left, the Updates disclosure with the comparison and its dates,
  then previous and next change, previous and next file, and private progress
  as an icon;
- Compose review and Comment as icons on its right, with tooltips naming the
  command and its key, and Compose review marking a kept review draft.

Review details moves into the comparison's menu. Option-R (Alt+R), Option-C
(Alt+C) and Esc back from a composition page are bound, and both composition
commands join the Pull Request menu.

## Seams

- `build_central_surface` and the strip builder in
  `src/adapters/qt/src/workbench_window.cpp` and `workbench_strip.cpp`.
- The epoch comparison control's menu, for Review details.
- The Pull Request menu in `workbench_window.cpp`.
- The review-submission workflow's draft state, projected for the kept-draft
  mark.
- `composeReviewAction` and `composeInlineDraftAction` keep their object names
  and become window-owned, so their keys work without a visible button.

## Tests that change with it

- BPR-0940's allow-list in `test_the_epoch_row_spans_the_window_in_every_modality`.
- The declared focus order and its unique-shortcut check.
- The UIforETW capture's exact strip row and its height floors, in
  `github_application.cpp` and in
  `tests/end_to_end/application_uiforetw_selected_diff_contract.cmake`, with
  the digests `application_review_round_trip_composition_contract` pins.
- The Windows width sweep at the 902 floor.

## Acceptance

- The chrome is the title row, the review bar, then the Updates line's
  timeline while open, and nothing else sits between them in any state or
  modality.
- The title row holds only the listed controls, with the modality at its
  right; every control stays top level at each platform's width floor, swept
  again on Windows.
- The review bar reads Updates and the comparison, then the active tab's
  controls, from its left, with the two icons on its right. Tooltips name each command and key; Compose review marks
  a kept draft; Review details opens from the comparison's menu.
- Option-R, Option-C and Esc work, both commands are in the Pull Request menu,
  and no key sequence is used twice.
- The height floors are re-taken from the measured stacks on each platform,
  with the Updates line open and folded.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-01, AC-02, AC-03, AC-10, AC-11 as drawn in [round v8](../../design/BPR-1106-scaffolding-v8.html). The
core decides, the host performs, Qt presents. It changes no publication,
authority or review semantics and no provider, Git or storage capability. It
starts in canonical order, after RFC-BPR-0028 to RFC-BPR-0038 close. Run the
focused courses named here and the documentation gate, and record exact
commands, results and the producing revision. The owner's attended look at
the built chrome on macOS and Windows is evidence this card cannot supply by
itself.
