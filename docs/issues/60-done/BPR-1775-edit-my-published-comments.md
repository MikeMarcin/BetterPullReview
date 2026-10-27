---
schema: "implement-rfc.card.v1"
id: "BPR-1775"
title: "Edit my published comments"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1774--BPR-1775--77f10df92a48.json","kind":"feature","found_by":"owner chat, 2026-10-26","related_cards":["BPR-1774","BPR-1368","BPR-1741","BPR-1779"],"frontend_design":"frontend-design:frontend-design loaded before the Qt surface"}
acceptance: [{"criterion": "CE01 who sees Edit", "outcome": "pass", "revision": "64822fb46", "evidence": {"kind": "command", "command": "ctest -R '^(core_comment_edit|qt_conversation_cards_view)$'", "detail": "Coordinator decision replaces the draft's viewerCanUpdate offer: Edit goes to the viewer's own published comments (the read's Mine). core_comment_edit: comment_edit_subject_of names review-comment, issue-comment and review posts and nothing else; comment_edit_offered only for mine in a signed-in read. qt_conversation_cards_view test_my_comments_are_edited_in_place: no Edit without the composition's offer; my reply offers Edit among its tools and in its body menu, someone else's offers neither; my pull request comment is editable; in Review (not the feed) too. Live: the second identity is offered no Edit on the owner's three comments, nor the owner on the second identity's."}}, {"criterion": "CE02 core decisions", "outcome": "pass", "revision": "64822fb46", "evidence": {"kind": "command", "command": "ctest -R '^core_comment_edit$'", "detail": "decide_comment_edit in order not_mine (and anonymous), unchanged (bytes: CRLF to LF is a change), empty (blank), too_long (65,536 code points kept, one more refused, four-byte characters count one); decide_comment_save: already saved before not permitted before conflict before write."}}, {"criterion": "CE03 fixed GraphQL documents, both connections", "outcome": "pass", "revision": "64822fb46", "evidence": {"kind": "command", "command": "ctest -R '^(github_mutation_contract|gh_cli_transport|github_read_contract)$'", "detail": "test_a_published_comment_goes_by_graphql_exactly: ObserveComment and EditReviewComment (updatePullRequestReviewComment), EditIssueComment (updateIssueComment), EditReviewBody (updatePullRequestReview) compile to exact bodies naming the comment's node; revalidate canonically; a read passed off as the write, a body changed in flight, another comment and another kind's update refused; unbound, foreign, nodeless, malformed-node and other-actor requests refused; 65,536 characters fit; GHES 3.18 at /api/graphql; answers held to the node, its GraphQL type and the designated pull request; viewerDidAuthor false is held not permitted; the write's answer must carry the body asked. Every operation row and request-shape pin covered on both servers. gh_cli_transport: all four documents for each kind (and a 65,536-character write) launch gh api graphql with the exact body on stdin; a body naming another comment never launches; a lost write stays possibly sent. The three mutation names are reserved in the read contract."}}, {"criterion": "CE04 exactly one write, conflict never overwritten", "outcome": "pass", "revision": "64822fb46", "evidence": {"kind": "command", "command": "ctest -R '^(live_review_mutation_provider|application_review_mutation_application_host)$'", "detail": "test_a_published_comment_is_observed_then_written_once: each kind observed then written once by its own update; another kind's node, another pull request, viewerCanUpdate false, and another account's comment a maintainer may update are refused before any write; a changed body is a conflict with no write and the newer body back; a body GitHub already has is not written; a write answer naming another pull request is refused without a second write; a lost answer settled by one read back (confirmed or not_confirmed); the coordinator confirms the target first. Application host: refused before any effect without the capability, for unchanged, blank, too long or nameless, another session's grant or a publication's one-shot grant; admitted edit queued and reaching the provider once bound to target, actor, kind and node; shutdown stops a queued edit."}}, {"criterion": "CE05 shipped composition course", "outcome": "pass", "revision": "64822fb46", "evidence": {"kind": "command", "command": "ctest -R '^(application_comment_edit_contract|application_description_edit_contract|application_threaded_reply_contract|application_review_round_trip_composition_contract|application_conversation_read_public_path)$'", "detail": "application_comment_edit_contract runs --threaded-reply-probe --comment-edit-probe on github-com and ghes-3.18 fixtures: COMMENT_EDIT_EVIDENCE:11111 (the thread's first post given to another account offers no Edit, my published reply offers it in Discuss; Save performed exactly one write (edits=1 writes=1); the next read shows GitHub's body; a body another client changed meanwhile is refused with nothing written (edits=2 writes=1), the typed text kept; Edit GitHub's newer text loads it). The fixture course exercises the review-thread comment kind; the pull request comment and review body kinds are covered by the contract, provider and live courses. Round-trip composition digests resealed."}}, {"criterion": "CE06 live course", "outcome": "pass", "revision": "64822fb46", "evidence": {"kind": "command", "command": "python3 tests/scripts/comment_edit_live_course.py --tool BUILD/tests/github_contract/reaction_live_course --work DIR", "detail": "MikeMarcin/BPR-Test #139, 2026-10-27, through the shipped review_mutation_session::edit_comment over gh api: for the owner's review-thread comment, pull request comment and review body (all CRLF, kept by GitHub): saved writes=1, saved-again writes=0, stale original conflict writes=0, restored writes=1, read back byte for byte. MikeMarcinTest on the owner's three, and the owner on MikeMarcinTest's comment: offered=0, asked anyway refused_by_server writes=0. 28 tool requests; REST X-RateLimit-Remaining 4989 to 4969. A first run (#138) found the owner, a repository admin, could update MikeMarcinTest's comment (viewerCanUpdate true): the session wrote it once before the fix that holds it to viewerDidAuthor; that comment was deleted by the course's cleanup. The product's native HTTPS connection was not run live (contract and fixture evidence only)."}}, {"criterion": "CE07 owner look", "outcome": "pending", "revision": "64822fb46", "evidence": {"kind": "observation", "detail": "Deferred by the owner; not a blocker. Offscreen light renders (Edit offered, editor open, conflict) from conversation_cards_view_tests --render-bpr-1775 DIR are in the session scratchpad of 2026-10-27; no attended-review session was opened."}}]
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

## Decisions (coordinator, 2026-10-27, not reopened)

- Edit goes to comments the viewer wrote (the read's Mine), not by a
  `viewerCanUpdate` read; Save observes the comment by node first and refuses
  in words when GitHub says no. Kinds: review-thread comments, pull request
  comments, review bodies. Conflict: nothing sent, typed text kept, GitHub's
  newer text offered. 65,536 characters. Delete is out of scope.

## Built (2026-10-27)

- Core: `bpr/core/comment_edit.hpp` (`comment_edit_subject_of`,
  `comment_edit_offered`, `decide_comment_edit`, `decide_comment_save`).
  Blank text is refused: emptying a comment is not how GitHub deletes one.
- GitHub adapter: `ObserveComment` (type, body, `viewerCanUpdate`,
  `viewerDidAuthor`, `updatedAt`, pull request) and one update per kind:
  `EditReviewComment` (`updatePullRequestReviewComment`, the mutation the
  pending tour stop edit of [[BPR-1368]] uses; that path is unchanged),
  `EditIssueComment` (`updateIssueComment`), `EditReviewBody`
  (`updatePullRequestReview`). `review_mutation_session::edit_comment`
  observes, lets the core decide, writes once and settles a lost answer with
  one read back. The GitHub CLI connection admits all four through
  `gh api graphql`.
- Application: `review_comment_edit_request`, the port's `edit_comment`, the
  coordinator, and the host's `edit_comment` under a one-shot
  `issue_comment_edit` grant bound to the comment's node.
- Qt: on my own post, Edit in its tools (beside React, Source, Copy) and its
  body menu, in the feed and in Review; the reply's frame with
  `scoped_prose_editor` takes the body's place (the reply's own text field
  does not keep CRLF bytes, so the prose editor that does is used inside the
  reply's frame and buttons); Cancel and Save, Ctrl+Return saves, Escape
  cancels; a conflict offers Edit GitHub's newer text; a refresh keeps the
  edit. The open card redraws when a post's author changes.
- Composition: offer, core gate, host write, refresh after success. The post
  shows GitHub's answer through the ordinary read; the read carries no
  "edited" fact, so none is shown (none was added).
- Docs: product contract "Editing my comments"; provider contract.

## Found on the way

- GitHub lets a repository maintainer update another account's comment
  (`viewerCanUpdate` true on BPR-Test for the owner). The offer was already
  Mine-only, but the session would have written such a comment when asked
  directly; it now refuses a comment the viewer did not write.
- The open card's signature did not include a post's author, so a read that
  changed who wrote a post kept the old Edit offer; it does now.

## Known gaps

- The composition fixture course edits a review-thread comment only; pull
  request comments and review bodies are proven by the contract, provider
  and live courses, not by a composed fixture course.
- `application_review_round_trip_composition_contract` failed between the
  BPR-1779 follow-up commit and the composition commit (digests resealed in
  the latter).
- The native HTTPS connection was not run live.
- The owner's look is pending (deferred).
