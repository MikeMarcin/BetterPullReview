---
schema: "implement-rfc.card.v1"
id: "BPR-1130"
title: "The Updates line folds and loses its permanent caption"
status: "Ready"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1129"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner decision on BPR-1106, 2026-09-24","design":"docs/design/BPR-1106-scaffolding-v8.html","criteria":["AC-01","AC-04","AC-11"],"reservation":"docs/issues/_reservations/BPR-1129--BPR-1135--89a65c75ff1a.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
---

# BPR-1130: The Updates line folds and loses its permanent caption

## Outcome

The Updates line folds from the review bar's disclosure. Open, its timeline
runs alone across the whole window just below the bar; folded, it leaves no
row. The bar does not move either way, and it names the comparison and its
dates in both states. A new window starts folded, and each
window restores its own choice through the existing window restoration.
Nothing folds or opens on its own.

The permanent caption goes. The history statement becomes the line's tooltip
and accessible description, and a visible mark on the line says when history
is incomplete or ambiguous, the statement RFC-BPR-0029 requires the line to
make.

## Ask first

Before it changes the readiness line, this card asks the owner whether its
words should take the Updates line's track, or the review bar's summary when
folded, so that no row comes or goes above the comparison. The rounds drew
that from v2 on, but it was never asked separately. Without an answer,
[[BPR-0795]]'s row stays.

## Acceptance

- The fold works from the review bar and by keyboard. A new window starts
  folded, and each window restores its own state.
- There is no permanent caption. The tooltip carries the history statement,
  and an incomplete or ambiguous history shows a mark on the line in every
  modality.
- The height floors hold with the line open and folded on each platform.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-01, AC-04, AC-11 as drawn in [round v8](../../design/BPR-1106-scaffolding-v8.html). The
core decides, the host performs, Qt presents. It changes no publication,
authority or review semantics and no provider, Git or storage capability. It
starts in canonical order, after RFC-BPR-0028 to RFC-BPR-0038 close. Run the
focused courses named here and the documentation gate, and record exact
commands, results and the producing revision. The owner's attended look at
the built chrome on macOS and Windows is evidence this card cannot supply by
itself.
