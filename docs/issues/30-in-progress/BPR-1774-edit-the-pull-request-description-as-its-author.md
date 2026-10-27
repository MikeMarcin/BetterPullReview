---
schema: "implement-rfc.card.v1"
id: "BPR-1774"
title: "Edit the pull request's description (summary) as its author"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1774--BPR-1775--77f10df92a48.json","kind":"feature","found_by":"owner chat, 2026-10-26","related_cards":["BPR-1775"]}
acceptance: []
---

# BPR-1774: Edit the pull request's description (summary) as its author

## Request

The owner, 2026-10-26, verbatim:

> We seem to be missing author/uploader controls for editing the summary and
> we seem to be missing controls for editing comments we have left. Add
> cards for both of those and add them to your work queue

This card is the first half: the summary. [[BPR-1775]] is the comments.

## Grounded current behavior (to confirm when the card starts)

- The description is read and rendered (Overview), but no surface offers to
  change it. A search of `src/` finds no `updatePullRequest` mutation and no
  description editor; the mutation catalog's update operations are for
  pending review comments ([[BPR-1368]]) and tour staging only.
- GitHub lets the author and anyone with write access edit the title and
  body (`updatePullRequest`, GraphQL, or `PATCH /repos/{o}/{r}/pulls/{n}`).
  The read model would need the viewer's permission to show the control only
  to them (`viewerCanUpdate`).

## Outcome

- Whoever GitHub lets edit it sees **Edit** on the description in Overview;
  others see nothing new.
- Editing opens the existing Markdown composer on the description's raw
  body, with Save and Cancel. Nothing is sent until Save.
- Save sends one fixed mutation through the connection in use (native and the
  GitHub CLI connection), and the description shows GitHub's answer through
  ordinary synchronization. A body changed on GitHub since editing began is
  not overwritten silently: the editor says so and offers to keep editing on
  the newer text.
- Refusals say why in words (permission, connection, conflict).
- Out of scope unless the owner asks: the title (consider beside this),
  labels, reviewers.

## Acceptance (draft)

- The control is offered exactly when GitHub grants the viewer the edit.
- Save performs exactly one update on fixtures for github.com and GHES 3.18,
  and a live course on MikeMarcin/BPR-Test.
- A concurrent change is detected and not overwritten.
- The look is accepted by the owner on a frozen build (attended review).
