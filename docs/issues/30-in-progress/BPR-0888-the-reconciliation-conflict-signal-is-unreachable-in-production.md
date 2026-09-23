---
schema: "implement-rfc.card.v1"
id: "BPR-0888"
title: "The reconciliation conflict signal is unreachable in production and misses the delete-versus-local-edit case"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable core+application partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0888: The reconciliation conflict signal is unreachable in production and misses the delete-versus-local-edit case

## Defect

`reconciliation_kind::conflicted` cannot occur outside the unit tests, and the
one case it most needs to cover is not modelled at all.

- `src/application/src/review_synchronization.cpp:96-108`: a comment absent
  from the server is classified `removed` **regardless** of whether it is in
  `aLocallyEdited`. `conflicted` is produced only for `updated` plus edited
  (`:74-88`).
- The only production caller passes an **empty** edited set:
  `src/app/src/github_application.cpp:8235-8243` calls
  `application::reconcile(ReviewProjection, ..., { })`. So no conflict of any
  kind can be reported in the shipped product; the kind exists solely for
  `tests/application/review_synchronization_tests.cpp`.

The function's own comment says "a conflict is reported, not resolved", which
is exactly what does not happen.

Counterexample, at the unit level:
`reconcile(local={a}, observed={}, edited={a})` yields one `removed` entry and
no conflict. The caller cannot distinguish "the server deleted it" from "the
server deleted it while you had unpublished text in it" — and the second is a
case where a reviewer loses work silently.

The app wiring is the sibling half; the application-side contract gap is the
part this card is grounded in.

## Scope

- Decide the model first: either a `removed_conflicted` kind, or an
  edited-aware `removed` that carries the fact. The choice matters because it
  determines what the surface has to show.
- Wire the edited set from the draft store to the production caller, so the
  kind is reachable at all.
- Give the reviewer somewhere to see it. A conflict signal that reaches no
  surface is the same defect one layer up.
- Add the counterexample above as a contract row.

## Acceptance

- A comment deleted on the server while carrying an unpublished local edit is
  reported as a conflict, through the production path and not only in tests.
- The reviewer can see that it happened and what the local text was.
- The existing `updated`-plus-edited conflict still holds.

## The deletion case is a conflict, and the production premise was stale (bpr-main-mac, Mac batch 5, 2026-09-23)

Station `bpr-main-mac` (Apple Silicon, AppleClang, Qt 6.11.2, `build/dev`
Debug), claimed at `4a22481b`.

**Re-check at HEAD.** Both halves of the defect hold as described. `reconcile`
reported an edited comment that the server deleted as `removed`
(`review_synchronization.cpp`, the removal loop). The only production caller,
`refresh_review_projection` (`github_application.cpp` ~8656-8666), passes an
empty edited set in both its fixture and production branches.

**But the production premise is false.** The card assumes a draft store that
holds unpublished edits of server comments, waiting to be wired to the
caller. No such store exists, because the product never edits a published
comment:

- A draft is a new comment or a reply to a thread
  (`revisioned_review_draft_command::ReplyToThread`). The GitHub mutations are
  create, reply-to-thread, resolve and unresolve. There is no
  `updatePullRequestReviewComment` or REST comment edit anywhere in `src`.
- `docs/architecture/review-round-trip.md`, section Drafts, says so on
  purpose: "Once published, no draft command touches the comment. Editing a
  published comment is a different operation against a different authority."
- The nearest real case is a reply draft whose thread disappears. That
  publication fails, and a failed publication "returns the draft to
  composition with its text intact" (same section). No text is lost silently.

So the empty edited set is accurate for the shipped feature set, and there is
nothing on the production side to wire or show. Building the edit feature so
that the kind becomes reachable would be a product feature with a live
mutation, not a defect fix.

### Amendment to the acceptance (owner mandate, 2026-09-23)

The owner's mandate for this run says "You can revise and update the cards and
rfcs if they have gone stale." Under it, criteria 1 and 2 are revised because
their premise is stale. The original text stays above.

- **AC1, revised.** `reconcile` reports a comment that the server deleted while
  it carried an unpublished local edit as a conflict, and a contract row
  asserts it. Was: "…reported as a conflict, through the production path and
  not only in tests."
- **AC2, revised.** The architecture records that no shipped feature holds such
  an edit, that the production refresh therefore passes an empty edited set,
  and what a feature that adds editing of published comments must do: pass its
  edited set, and give both conflict kinds a surface that shows the held text.
  Was: "The reviewer can see that it happened and what the local text was."
- **AC3, unchanged.** The existing `updated`-plus-edited conflict still holds.

The change is platform-independent application logic with no I/O. The
Linux and Windows runs listed below confirm it and may follow the finish.

### The model

A distinct kind, `reconciliation_kind::removed_conflicted` (name
`removed_conflicted`). The alternative, reusing `conflicted`, would make a
reader infer a deletion from the comment's absence in the projection. The kind
says it directly. `conflicts()` lists both kinds. The comment still leaves the
projection: the server remains the authority.

- `src/application/include/bpr/application/review_synchronization.hpp`: the
  enumerator, with a note on what the two conflict kinds mean;
  `conflicts()` is documented; `reconcile`'s contract covers deletion and says
  the production caller passes no edited ids today.
- `src/application/src/review_synchronization.cpp`: the removal loop reports
  `removed_conflicted` for an edited id; `name` and `conflicts()` cover the
  new kind.
- `docs/architecture/review-round-trip.md`: the Delta list, the deletion rule,
  and the production note with the obligation for a future edit feature.

No other code switches on `reconciliation_kind`. The Qt draft editor reads only
the projection's size. `github_application.cpp` is untouched, so the
round-trip contract's pins do not move.

### Contract rows (`application_review_synchronization`)

- `test_a_deletion_under_a_local_edit_is_a_conflict`: the card's
  counterexample. `reconcile(local={a}, observed={}, edited={a})` gives an
  empty projection, one entry `a: removed_conflicted`, and
  `conflicts() == {a}`.
- `test_both_conflicts_are_listed_together`: a changed edited comment, a
  deleted edited comment and a deleted unedited comment report `conflicted`,
  `removed_conflicted` and `removed`. `conflicts()` is `{a, b}`.
- `test_every_enumerator_names_itself` now covers `removed_conflicted`.
- The existing `test_a_local_edit_never_overwrites_an_external_change` (AC3)
  and `test_an_external_deletion_removes_the_comment` are unchanged and pass.

**Negative controls**, each a temporary edit of the final
`review_synchronization.cpp`, restored afterwards and re-hashed:

| Break | Result |
| --- | --- |
| The removal loop always reports `removed` | `FAILED: the deletion is reported as a conflict rather than a plain removal` |
| `conflicts()` lists only `conflicted` | `FAILED: the deleted comment is listed so a person can decide about the edit` |

### Verification (macOS)

| Check | Result |
| --- | --- |
| `cmake --build --preset dev` | Exit 0, no `warning:` line |
| `ctest --test-dir build/dev -j1 --output-on-failure -R '^(application_review_synchronization\|application_review_round_trip_composition_contract\|application_review_ledger_composition_contract)$'` | 3/3 passed (0.04 s, 11.21 s, 36.02 s; load 31 at the start). The two courses run the production `reconcile` call after a fixture publication |
| clang-format 23.1.1 on the three C++ files | Clean |
| `_coding-standard/scripts/tidy.py` on `review_synchronization.cpp` and its test | No findings |
| `scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

### Confirmation runs owed (Linux, Windows)

- **Linux (bpr-main-linux):** `cmake --build --preset dev && ctest --test-dir build/dev -j1 --output-on-failure -R '^application_review_synchronization$'`
- **Windows (bpr-main-win):** `cmake --build --preset windows-dev && ctest --preset windows-dev -j1 --output-on-failure -R '^application_review_synchronization$'`

Expected on each: `review-synchronization-tests:pass`.
