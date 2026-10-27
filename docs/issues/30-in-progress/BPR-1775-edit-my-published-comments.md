---
schema: "implement-rfc.card.v1"
id: "BPR-1775"
title: "Edit my published comments"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1774--BPR-1775--77f10df92a48.json","kind":"feature","found_by":"owner chat, 2026-10-26","related_cards":["BPR-1774","BPR-1368","BPR-1741"]}
acceptance: []
---

# BPR-1775: Edit my published comments

## Request

The owner, 2026-10-26, verbatim:

> We seem to be missing author/uploader controls for editing the summary and
> we seem to be missing controls for editing comments we have left. Add
> cards for both of those and add them to your work queue

This card is the second half: comments. [[BPR-1774]] is the summary.

## Grounded current behavior (to confirm when the card starts)

- A pending (unpublished) review comment can be edited ([[BPR-1368]]:
  `updatePullRequestReviewComment` for a pending comment, the one GraphQL
  write the gh connection admitted before [[BPR-1741]]).
- A comment once published offers no Edit: not on review-thread comments,
  pull-request (issue) comments or review bodies. The tour staging contract
  has `update_issue_comment`, but no reviewer surface uses it.
- GitHub lets a comment's author edit it (`viewerCanUpdate` on each comment)
  and records an edit history ("edited").

## Outcome

- On a comment I wrote that GitHub lets me update, the post's menu (and the
  open card) offers **Edit**. Editing opens the comment's raw Markdown in the
  same composer a reply uses, with Save and Cancel.
- Save sends one fixed update for the comment's kind: review comment
  (`updatePullRequestReviewComment`), pull-request comment
  (`updateIssueComment`) or review body (`updatePullRequestReview`), on the
  native and GitHub CLI connections, and the post shows GitHub's answer
  through synchronization, marked edited as GitHub marks it.
- A comment changed on GitHub meanwhile is not overwritten silently.
- Delete of a published comment is a separate decision for the owner: ask
  before adding it (it is irreversible on GitHub).

## Acceptance (draft)

- Edit is offered exactly for comments the viewer may update.
- Each kind performs exactly one update on fixtures (github.com, GHES 3.18)
  and on MikeMarcin/BPR-Test with a second identity proving another person's
  comment offers no Edit.
- Concurrent edits are detected.
- The look is accepted by the owner on a frozen build (attended review).
