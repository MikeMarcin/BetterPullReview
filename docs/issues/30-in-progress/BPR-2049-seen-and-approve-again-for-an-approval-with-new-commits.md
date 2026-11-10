---
schema: "implement-rfc.card.v1"
id: "BPR-2049"
title: "Seen, or Approve again, for an approval that new commits brought back to Needs me"
status: "In Progress"
rfcs: ["RFC-BPR-0098"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2048--BPR-2049--1f6f243721c8.json","origin":"owner request, 2026-11-10"}
depends_on: []
---

# Seen, or Approve again, for an approval that new commits brought back to Needs me

## Origin

The owner, 2026-11-10: "So if I approved something already and it gets new
commits it moves back to needs me which makes sense but then I don't know how
to say okay I still accept it so it clears new commits and moves back to
following."

## Scope and acceptance

Agreed design: while GitHub still holds the owner's approval, the row (and
the pull request) offers Seen, which records the new commits as seen locally
and moves the row back to Following with no GitHub write. When GitHub
dismissed the approval (a stale-review dismissal), the row offers Approve
again, which submits a real approval; the row moves once GitHub confirms. The
row moves at once after Seen, not at the next refresh. No Unapprove action
(the owner declined one).

Load frontend-design before the Qt work; the owner's look on a frozen build
is filed, not waited on.
