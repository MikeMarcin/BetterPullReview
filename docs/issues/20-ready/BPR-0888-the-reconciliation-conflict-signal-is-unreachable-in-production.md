---
schema: "implement-rfc.card.v1"
id: "BPR-0888"
title: "The reconciliation conflict signal is unreachable in production and misses the delete-versus-local-edit case"
status: "Ready"
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
