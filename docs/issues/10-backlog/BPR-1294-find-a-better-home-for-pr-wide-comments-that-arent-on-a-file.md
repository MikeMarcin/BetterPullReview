---
schema: "implement-rfc.card.v1"
id: "BPR-1294"
title: "Find a better home for PR-wide comments that aren't on a file"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner chat, 2026-10-06, during BPR-1290","reservation":"docs/issues/_reservations/BPR-1294--BPR-1294--35ad4cad03a9.json","related_rfcs":["RFC-BPR-0028","RFC-BPR-0049"],"related_cards":["BPR-1290","BPR-1293","BPR-0648","BPR-0649","BPR-1089","BPR-0211"],"implementation_note":"Use the frontend-design skill when designing; owner review is required","leading_direction":"On Overview, as in SmartBear Collaborator's review overview"}
---

# BPR-1294: Find a better home for PR-wide comments that aren't on a file

## Origin

While redesigning the Conversations drawer ([[BPR-1290]]), the owner said in
chat on 2026-10-06, verbatim:

> I guess there's comments that are on the PR and not tied to source files to
> consider. I'm not sure how we deal with those at all right now.

The plan then moved them, with the conversation finder, into a view you
summon ([[BPR-1293]]). The owner narrowed that, verbatim:

> Well I dont necessarily want to summon non-file comments like that. Just the
> navigation I was thinking. We might need a different better solution for PR
> wide comments that arent on a file.

And, verbatim:

> I know in collaborator those comments are effectively on their version of the
> overview page.

This is an open problem. Nothing is designed yet.

## Leading candidate direction

PR-wide comments live on **Overview**, as they effectively do on SmartBear
Collaborator's review overview (the owner's reference product, [[BPR-0211]]).
Overview is a mode the owner intends to keep: "beyond overview and review of
course" (2026-10-06, on how long the modalities stay). Confirm this direction
with the owner before designing.

## What "not on a file" covers

GitHub has three kinds (`application::github_conversation_kind`):

- **Pull request comments** (`issue_comment`): the conversation tab's comments.
- **Review summaries** (`review`): the body a reviewer writes with Approve,
  Request changes or Comment. It may be empty.
- **Review threads that cannot be placed**: threads whose anchor is
  unavailable in the shown comparison. They are on a file, but no line shows
  them. Whether they belong here or with their file is part of the question.

A file-level comment (`github_conversation_remote_subject::file`) is on a file
but not a line. It stays with its file (BPR-1290), not here.

## How they are shown today

Read from the code at `fbbabb3ef`:

- **The drawer's "All PR conversations" scope** (`github_conversation_panel`,
  `ScopeWholePr`; [[BPR-0648]], RFC-BPR-0028 AC-05). In compact mode,
  `conversation_cards_view` admits every top-level kind, so pull request
  comments and reviews appear as entries in file order, among the threads.
  "This file" admits only the file's own threads, so in Read the only way to
  reach a pull request comment is to switch the scope. BPR-1290 removes the
  scope from the drawer (inline only), which leaves these with no home in Read.
- **Discuss** ([[BPR-0649]]). The same list, widened; the selected post's feed
  (`github_conversation_model`) heads each one "Pull request comment by
  @name" or "Review by @name", with no file or line
  (`anchor_text` is empty for both).
- **Overview, header** (`github_pull_request_panel`). Only a count: "… · n
  reviews · n review threads · n pull request comments", beside the pull
  request's description. No comment is readable there.
- **Overview, Comments column** (contract only: the delivery contract's
  [Not on a file](../../ux/conventional-comments-feedback.md#overview-comments-column-bpr-1087-bpr-1088-bpr-1089-rfc-bpr-0049)
  row; [[BPR-1089]], Backlog). A last row holds unanchored threads, pull
  request comments and review summaries as counts (`Unattributed` in
  `core::feedback_partition`). Its drill-down opens Discuss. The Qt row is
  not built.
- **Links.** `#pullrequestreview-<id>` reveals that review in Discuss. An
  `#issuecomment-` link opens the pull request and says "A single discussion
  comment is not shown from a link." (`pull_request_location`; workbench
  specification, "What a link names").
- **Writing.** Replying to or adding a pull request comment, and a review
  summary at Submit (`review_submission_editor`), are outside this inventory's
  reading question. Check them when this card is taken up.

## Not decided here

Where they go, how they read and reply, how they count as new, how they relate
to Overview's Comments column and its "Not on a file" row, and what a link to
one opens. Ask the owner with drawings, starting from the Collaborator
direction above.

## Acceptance

To be written with the owner when this card is taken up. Owner review is
required, not self-certified.
