---
schema: "implement-rfc.card.v1"
id: "BPR-0668"
title: "A failed comparison leaves an empty workbench that says nothing is open"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"With the comparison unavailable, the workbench names the open pull request, lists its files, explains the one thing that failed and offers the retry","outcome":"pass","revision":"4b1d16f8","evidence":{"kind":"observation","detail":"qt_composed_github_scale_{100,125,200} pin that a pull request with an overview and no comparison lists its own files in the drawer, and that the line reads \"This pull request is open. Its comparison is not ready; Refresh to try again.\" The epoch and failure sentences the line already appended still say which part failed. Dropping the overview-fed rows fails the files assertion. The retry named is the strip's existing Refresh; the centre panes still read \"Select a file comparison\", which the card's notes record as not done here."}},{"criterion":"The false \"no pull request is open\" line cannot be produced while a pull request is open; a test pins it","outcome":"pass","revision":"4b1d16f8","evidence":{"kind":"observation","detail":"The sentence is now reachable only when no pull request is open, and the status is rewritten when a pull request opens or closes rather than only when a workbench projection lands. The test asserts the sentence is absent while one is open; restoring the old branch fails it. Fifty-nine Qt, composition, round-trip and public-path selectors pass, with check_boundaries, strict tidy, clang-format --Werror and verify_documentation.py."}}]
---


# A failed comparison leaves an empty workbench that says nothing is open

With [BPR-0667](../40-parked/BPR-0667-the-epoch-refuses-any-pull-request-whose-base-branch-moved.md)'s
acquire failure, the owner was left looking at a workbench with two empty code
panes, an empty file tree, "Select a file comparison" in the centre, and this
status line:

> No pull request is open in the workbench. The workbench could not be updated.
> Review history could not be updated. The exact comparison is unavailable.

A pull request *was* open: the strip named it, the drawer showed its title,
author, branches and merged state, and the conversations drawer listed seven
comments. The first sentence is false, and the remaining three name three
failures without naming one thing the reviewer can do. Nothing offers a retry.

This is the failure a reviewer meets first, and it must not read as "the
product is broken and you are stuck".

## Scope

- The status line says what is true: which part failed, and what still works.
  Never "no pull request is open" while one is open.
- A comparison that could not be prepared offers the retry the refusal already
  supports, in the surface the reviewer is looking at.
- The files list comes from the pull request's own file set, so a failed
  comparison leaves the files visible and the panes explained rather than both
  empty.
- Owned paths: `src/adapters/qt` workbench window status and empty states,
  the projections that feed them, their tests.

## Acceptance

- With the comparison unavailable, the workbench names the open pull request,
  lists its files, explains the one thing that failed and offers the retry.
- The false "no pull request is open" line cannot be produced while a pull
  request is open; a test pins it.

## Notes (2026-09-07)

- **The false line.** `update_status_text` read the absence of a cursor,
  which is a position inside a comparison, as the absence of a pull
  request. It also never ran when a pull request opened or closed, only
  when a workbench projection landed, so the line could be stale as well
  as wrong. It now states the open pull request first: "This pull request
  is open. Its comparison is not ready; Refresh to try again.", and the
  old sentence survives only for the case it describes, a workbench with
  no pull request. The refresh is written when what is open changes,
  guarded because that runs while the window is still being assembled.
- **The empty drawer.** The files tree was fed only by the comparison, so
  a pull request whose comparison failed showed no files beside a
  conversations drawer listing seven comments. The overview projection
  already carries the pull request's own file set, and the tree is filled
  from it when the comparison brought no rows. Opening one still needs the
  comparison; the status line says so.
- **Not done here.** The centre's two panes still read "Select a file
  comparison" rather than naming the failure, and the retry is the strip's
  Refresh rather than a control in the empty pane. The line names it, which
  is what the owner was missing; a pane that explains itself is worth its
  own card if the wording still reads poorly in the running application.
- **Evidence.** `qt_composed_github_scale_{100,125,200}` pin that a pull
  request with an overview and no comparison lists its files, that the
  line never claims nothing is open while one is, and that it names
  Refresh. Fifty-nine Qt, composition, round-trip and public-path
  selectors pass. `check_boundaries`, strict tidy on the touched
  translation unit, `clang-format --Werror` and `verify_documentation.py`
  pass.
- **Negative mutations.** Restoring the old branch fails "it says what is
  true and names the way to try again"; dropping the overview-fed rows
  fails "the files the pull request names are listed without a
  comparison".
