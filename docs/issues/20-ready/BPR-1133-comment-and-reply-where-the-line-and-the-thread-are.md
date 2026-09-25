---
schema: "implement-rfc.card.v1"
id: "BPR-1133"
title: "Comment and Reply where the line and the thread are"
status: "Ready"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1129"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner decision on BPR-1106, 2026-09-24","design":"docs/design/BPR-1106-scaffolding-v8.html","criteria":["AC-08","AC-10"],"reservation":"docs/issues/_reservations/BPR-1129--BPR-1135--89a65c75ff1a.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
---

# BPR-1133: Comment and Reply where the line and the thread are

## Outcome

Comment, the review bar's icon and Option-C (Alt+C), starts a comment on the
current line or a reply to the selected thread. Its menu names the target:
"Comment on line 431…" or "Reply to tbutler's thread…". Its arrow is the
place RFC-BPR-0048's labels will be offered. Until then it offers the plain
comment only, and no label is invented here.

Thread cards beside the code offer Reply, as the accepted Read asset draws
them. The reply goes through the path the Discuss feed's Reply already uses.
They stop offering Comment here, which starts a second thread on the same
line.

## Acceptance

- Comment opens the inline page on the current line's draft, or on a reply
  draft for the selected thread.
- A card beside the code replies to its own thread.
- Nothing is published without the existing explicit Publish.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-08, AC-10 as drawn in [round v8](../../design/BPR-1106-scaffolding-v8.html). The
core decides, the host performs, Qt presents. It changes no publication,
authority or review semantics and no provider, Git or storage capability. It
starts in canonical order, after RFC-BPR-0028 to RFC-BPR-0038 close. Run the
focused courses named here and the documentation gate, and record exact
commands, results and the producing revision. The owner's attended look at
the built chrome on macOS and Windows is evidence this card cannot supply by
itself.
