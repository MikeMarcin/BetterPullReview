---
schema: "implement-rfc.card.v1"
id: "BPR-1345"
title: "Start a review: pending drafts that the review submission sends as review comments"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"BPR-1212 owner look, session aae8f3fcd307cfca3bfb8ae0b28f1b83, start-a-review-v001 Accept","decision_id":"dc7e0a01-68f0-42b2-8d34-ede92665e96c","reservation":"docs/issues/_reservations/BPR-1345--BPR-1345--530408819266.json","design":"docs/design/BPR-1212-start-a-review-v1/index.html","related_cards":["BPR-1212","BPR-1085","BPR-1329","BPR-1294"],"related_rfcs":["RFC-BPR-0037","RFC-BPR-0052"],"timing":"Build after BPR-1212's right-click v002 slice (coordinator, 2026-10-08)"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4ec36997e","evidence":{"kind":"observation","detail":"Drawer foot offers Add single comment (today's Publish bytes and key) and Start a review / Add review comment; a pending draft survives restart and is never published by navigation, local acceptance or another draft's single comment (application_start_review_contract pending and finish courses). Owner Accepts: write-a-comment v001 (decision 8af7409a, session 7f4f6349) and drawer-actions-keys v002 (decision 97b6bfee, session 6b69681e)."}},{"criterion":"AC2","outcome":"pass","revision":"4ec36997e","evidence":{"kind":"observation","detail":"Review chip shows 'Review · N pending' with Finish review, N exact, each pending draft drawn at its line (qt_workbench_line_draft, pending course). Owner Accept of review-in-progress v001 (decision 4a7fc8c1, session 7f4f6349)."}},{"criterion":"AC3","outcome":"pass","revision":"4ec36997e","evidence":{"kind":"observation","detail":"One GitHub review carrying exactly the pending drafts, summary rule, publication only on acceptance, refusal and unknown keep every draft (finish, refused, uncertain, again courses; live_review_submission_provider; github_mutation_contract). Discard review asks naming the count, deletes and closes the page; verdicts submit directly; a confirmed review closes the page. Owner Accepts in session 15cc2ae5: delete-comment v003 (81a1f1f7, with the cannot wording applied in 4ec36997e, no follow-up review per the owner), discard-review v003 (93a50f80), finish-closes v003 (2a220b19)."}},{"criterion":"AC4","outcome":"pass","revision":"4ec36997e","evidence":{"kind":"command","detail":"Composed public path over the fixture server and the real composition root (application_start_review_contract, review submission public path); failing-before controls per rule for v001, v002, v003 and the wording change; 100 ms gates pass (v002, input path unchanged since); round-trip pins resealed in v003 and matching at 4ec36997e; one live probe on MikeMarcin/BPR-Test#44. macOS rows; Windows and Linux rows owed by the stations."}},{"criterion":"AC5","outcome":"pass","revision":"4ec36997e","evidence":{"kind":"observation","detail":"One Accept/Reject question per criterion on exact frozen candidates in three short sessions (7f4f6349 v001, 6b69681e v002, 15cc2ae5 v003); every Reject rebuilt and later accepted; v003 handed back at event 14 with all three Accept."}}]
---

# BPR-1345: Start a review, with pending drafts sent as review comments

## Owner request

Building the compose drawer ([[BPR-1212]] slice 2), the owner wrote in chat on
2026-10-08, verbatim:

> I don't know how to publish my draft comment, compose review only lets me
> approve with an extra comment but I don't want to do that

> Perhaps what were missing is the equivalent of the GitHub start a review flow
> or if we have it I can't find it

Slice 2 made a single draft's Publish obvious and drew GitHub's second way as
[`BPR-1212-start-a-review-v1`](../../design/BPR-1212-start-a-review-v1/index.html).
In attended session `aae8f3fcd307cfca3bfb8ae0b28f1b83` the owner answered
`start-a-review-v001` ("Build the start-a-review flow as these frames show: Add
single comment or Start a review on the draft, a Review · N pending chip with
Finish review, and Finish review in Discuss where Comment needs no summary")
with **Accept** and no note: decision `dc7e0a01-68f0-42b2-8d34-ede92665e96c`,
event 20, handed back at event 21, fingerprint `5e57f224…`, candidate revision
`design-80bea4636-v1`.

The coordinator filed this card on 2026-10-08 as its own build milestone, to
start after BPR-1212's right-click v002 slice.

## What exists today (BPR-1212 slice 2 findings)

- A drawer draft has one way out: **Publish** (⌘↩), which posts that comment on
  its own at once.
- `review_submission` posts only `body`, `commit_id` and `event`. Compose
  review's Comment therefore needs a summary: GitHub refuses `COMMENT` with an
  empty body and no comments (422, probed on `MikeMarcin/BPR-Test#41`), and
  accepts it with one line comment and an empty body (`COMMENTED`).
- BPR-1085's preflight counts my selected unsent draft and refuses Submit when
  that selection changed after the reviewer chose it, but the submission does
  not carry the draft.
- Only the selected line's draft is drawn at its line.

## Intended work

As the frames show:

1. **Write a comment.** The drawer draft's foot offers **Add single comment**
   (⌘↩, today's Publish exactly) and **Start a review**, which keeps the draft
   pending, unpublished, in a review finished later.
2. **Keep adding.** While a review is in progress, the review bar shows
   "Review · N pending" with **Finish review**. On a new draft **Add review
   comment** becomes the main action, and every pending draft of the pull
   request stays drawn at its line, not only the selected line's.
3. **Finish in Discuss.** Finish review opens the summary in Discuss (BPR-1212
   Q3: A) listing the pending comments, with Comment, Approve and Request
   changes, an optional summary, **Submit review** and **Discard review**.
   Comment needs no summary when the review holds line comments.

Building it takes:

- A pending mark on a draft, kept with the draft on this computer (the private
  draft store), through core and application, with exact identity (account,
  pull request, head, line or range, draft revision).
- The review submission carrying the pending drafts as GitHub review comments
  (single lines and BPR-1212's `start_line`/`line` ranges, both sides), with
  publication identity that marks each draft published only when GitHub
  accepts the review, and a safe answer to an uncertain submission.
- The core's `validate_body` allowing an empty Comment summary only when the
  review carries at least one line comment.
- Add single comment keeping today's Publish bytes and idempotency key.

## Acceptance

- AC1: A drawer draft offers Add single comment (today's Publish, unchanged
  bytes and key) and Start a review / Add review comment. A pending draft
  survives restart with its pending mark and is never published by navigation,
  local acceptance or Add single comment on another draft.
- AC2: While any draft is pending, the review bar shows "Review · N pending"
  with Finish review, N exact, and each pending draft is drawn at its line.
- AC3: Finish review opens the summary in Discuss. Submit review sends one
  GitHub review whose comments are exactly the pending drafts (lines, ranges
  and sides), Comment with no summary is allowed only when it holds comments,
  and each draft is marked published only on GitHub's acceptance. A refusal or
  an uncertain answer keeps every draft and its text. Discard review, after
  a confirmation that names the count, deletes every pending comment of the
  review and closes the page (amended for v002 by the owner's discard-review
  v001 note; v001 cleared the marks and kept the drafts). The three verdicts
  submit directly (owner, finish-review v001).
- AC4: Composed public path through the fixture server and the real
  composition root, failing-before controls for each rule, the 100 ms
  interaction gates, the round-trip pin resealed. One live probe on
  `MikeMarcin/BPR-Test` only.
- AC5: Owner look at the built candidate, one Accept/Reject question per
  criterion.

## Scheduling

Ready, to be claimed after BPR-1212's right-click v002 slice is handed to the
owner. No remote mutation beyond the designated test repository.

## Build (bpr-main-mac, 2026-10-09)

Engineering owner `rfc-implementer` (`claude-opus-5-5`, effort high) on
`bpr-main-mac` (macOS, Apple clang, Qt 6.11.2, Debug `build/dev`, offscreen).
The drawer foot, the review bar's chip and the Discuss page were drawn with
`frontend-design`, loaded first, against the accepted frames
([`BPR-1212-start-a-review-v1`](../../design/BPR-1212-start-a-review-v1/index.html)).
Built on BPR-1212 v002 (`fc89f79e3`), rebased onto `0e0d9e5d7`.

### What was built

**The pending mark** (core). `review_comment_draft::InReview` is a
`review_pending_mark`: the file's path at the head the draft was written on,
that head, and, once a review is submitted, the submission's idempotency key.
`pending_in_review` is a drafting draft with a mark; `held_by_review` is a
publishing draft whose mark carries a key. The revisioned command sets or
clears the mark (`ChangePending`, `Pending`) in the same revision as its
command, and the core decides who may:
- Start a review (an edit) sets a keyless mark on a new comment with words, on
  either side; a reply never has one, and a draft that becomes a reply loses
  it. Edits keep it.
- Discard review (an edit) clears it. Neither touches a held draft.
- A review's Submit (begin_publish) gives the existing mark its key, on the
  same file and head; that holds the draft for that submission alone. Its
  answer completes it (complete_publish) or releases it (fail_publish with the
  key taken off, still pending). A held draft is released by nothing else,
  and is never edited, discarded or retried as a single comment.
- Add single comment (begin_publish) on a pending draft takes it out of the
  review in the same revision. A begin_publish that would leave a mark in
  place is refused, so a pending draft is never published on its own by
  accident.

**The review that carries them** (core). `review_submission_comment` is one
pending draft as a GitHub review comment: its draft and exact revision, path,
side, zero-based line and range start, and words. The review draft carries
them while it is written (`set_comments`) and Submit freezes them into the
proposal. `summary_required` is the rule the card asked of `validate_body`:
Request changes always needs a summary, Comment only when the review carries
no line comment, Approve never. At most 50 comments, one per draft, ranges
ending below their start, and the summary with every comment and path inside
56 KiB, so the one request stays inside the transports' 64 KiB. A comment
with no words is listed and refuses Submit. The prepared decision binds the
comments too (`review_preflight_change::comments`): a comment edited, added or
moved after Submit was pressed refuses that Submit.

**Storage** (SQLite). A pending draft is private-draft record version 3 (its
optional first line, then the mark); every other draft keeps its version 1 or
2 bytes. Storage refuses a mark on a reply or on a head the draft was not
written on. `change_drafts` applies several draft commands in one store
transaction, all or none. A review submission with comments is request
snapshot version 2; a summary-only one keeps version 1 and its fingerprint
(the comments are appended to the fingerprint only when present). `withdraw`
removes an unresolved attempt with its exact fingerprint once it is
definitely not published (the connection forbids trigger depth, so the
cascading parent row is deleted with foreign-key actions off for that one
transaction, child first); a resolved attempt is never removed.

**GitHub** (adapter). The review's one `POST /pulls/{n}/reviews` carries
`comments` between `body` and `commit_id`, each `{body, line, path, side}` and
for a range `start_line`/`start_side`, sorted keys; with none, the bytes are
exactly as before. The session marks it published only when the one new exact
review GitHub lists also holds exactly those comments, read from the pull
request's comment collection. A 422 answer followed by no new exact review is
a definite refusal, and that attempt is withdrawn rather than restored as
unfinished at every start; so is an attempt refused before its witness. Read
only: `admitted(key)` returns what was admitted under a key.

**Application and composition.** `review_mutation_application_host::
comment_destination` decides a pending comment's path and head exactly as
Publish validates a comment, without publishing (the current head, the
selected file, the source's lines, the base-line evidence). The review
submission host gains `lookup(key)`. The review submission workflow takes the
pending drafts as its comments, and on Submit asks the composition to hold
every carried draft at its exact revision, durably, before the host may send
anything; a draft changed since refuses the Submit. Its answer settles the
drafts: published completes each draft it carried at its held revision with
the same words; definitely not published releases them; an unknown answer
keeps them held until Check status. On restart, held drafts whose submission
is not the restored one are looked up by key: nothing admitted means nothing
was sent, so they are released; an admitted one is reconciled read only.
Discard review clears every mark and keeps the drafts. Add single comment
keeps today's Publish request and idempotency key.

**Qt.** The drawer draft's foot: the privacy note and Discard on one row, then
Add single comment (outlined) and the main action (filled): Start a review;
Add review comment (⌘↩) while a review is in progress, with "Pending until you
finish the review"; Update review comment (⌘↩, closes it still pending) on a
pending draft, whose header says Pending. The two stack in a narrow drawer.
Pending drafts are notes at their lines tagged "Pending in your review" in the writing blue ("Pending" where the drawer is narrow). The
review bar's chip, before Compose review: "● Review · N pending" on the
highlight's tint and Finish review on the highlight, one button, named and
described; "Review · submitting" while drafts are held. Finish review opens
"Finish your review" in Discuss: the pending comments by file, each with its
lines in the code font and its first line of words; "Submits N line comments
as one review."; "Leave a summary (optional)" for Comment and Approve; Discard
becomes Discard review.

### Live probe (MikeMarcin/BPR-Test#44, 2026-10-09)

One probe, 21 REST calls, `X-RateLimit-Remaining` read on every answer (4998
before, 4979 after). A base branch with a 30-line file and a head branch
removing two lines, changing one and adding three; pull request #44 between
them. The review body BPR compiled (`--compile-live`: a head line, a
three-line range of added lines, a removed base line, empty summary, Comment)
was accepted as `COMMENTED` with the three comments on their lines and sides.
A second review with a comment outside the diff got 422 "Line could not be
resolved" and the review list was unchanged. The pull request was closed and
both branches deleted.

Finding: GitHub's list of one review's comments
(`/reviews/{id}/comments`) carries no `line`, `side` or `start_line`, only the
legacy `position`; the pull request's comment collection carries them with
`pull_request_review_id`. The first build verified through the review's list
and would have left every real submission unknown; it now reads the
collection. `--replay-live` ran the production session over GitHub's saved
answers: the review was published with its real review id, and the 422 was a
definite refusal, withdrawn. (The staged tour path parses the review's own
comment list the same strict way; not this card's, noted for the tour's
owner.)

### Verification (macOS, Debug, offscreen)

New rows:
- `core_review_pending`: the mark's commands, the lock and its release, the
  summary rule, the comment bounds, the prepared decision.
- `application_start_review_contract` (`--start-review-course`, the composed
  application over the fixture server and the production session, four
  runs): `pending` `START_REVIEW_PENDING_EVIDENCE:11111` (Start a review keeps
  it pending with nothing sent; Add review comment by ⌘↩ adds a range; "Review
  · 2 pending"; both drawn; Add single comment on a third publishes exactly
  it; another file and back changes nothing); `finish` over the same data root
  after a restart `START_REVIEW_FINISH_EVIDENCE:11111` (both pending again;
  Finish review lists both; Comment with no summary; one POST whose comments
  are exactly the two; both marked published); `refused`
  `START_REVIEW_REFUSED_EVIDENCE:1111`; `uncertain`
  `START_REVIEW_UNCERTAIN_EVIDENCE:1111`.
- Extended: `application_comment_range_draft_store` (version 3, restart, one
  transaction or none, release, completion, another head refused),
  `sqlite_review_submission_recovery_storage` (version 2, fingerprint,
  withdrawal), `github_mutation_contract` (exact bytes with comments, the
  summary rule, refusals), `live_review_submission_provider` (exact comments
  published in any order; a single comment's review or another set is not
  it; 422 definite and withdrawn; `admitted`), `live_review_submission_restart_recovery`
  (an attempt killed before its witness is withdrawn on recovery),
  `application_review_submission_application_host` (lookup),
  `qt_conversation_cards_view` (the foot in three states, ⌘↩, the privacy
  note, pending notes, stacking), `qt_review_submission_editor_scale_{100,125,200}`
  (Finish your review), `qt_workbench_line_draft` (the chip),
  `qt_workbench_window_scale_{100,125,200}` (the bar's order).
- 124 focused rows pass after the rebase onto `0e0d9e5d7`, including the
  line-draft, label-authoring, threaded-reply, published-file-switch and
  review-submission public paths, the round-trip contract (resealed),
  `architecture_boundaries` and `test_network_isolation_manifest`;
  `architecture_boundary_contract` passed before the rebase.
- `coding-format` (clang-format 22) on every changed file; `DOCUMENTATION_OK`.

**Failing-before controls** (each mutation applied, the row built and run,
the file restored byte for byte):

| Mutation | Row | Failure |
|---|---|---|
| Start a review sets no mark | `application_start_review_contract` | `pending` waits at "first pending" until the budget |
| ⌘↩ publishes during a review | `qt_conversation_cards_view` | "and ⌘↩ adds the comment to the review" |
| A pending note is not marked | `qt_conversation_cards_view` | "a saved pending draft is a note at its line that says it is pending …" |
| The chip counts every draft | `qt_workbench_line_draft` | "two pending drafts: "Review · 2 pending" …" |
| The wire drops a range's first line | `github_mutation_contract` | "each pending draft is a review comment: one-based lines, both sides, a range's first line, sorted keys" |
| Comment always needs a summary | `core_review_pending`; `application_start_review_contract` | "Comment needs no summary with a line comment …"; `finish` waits at the page |
| A refusal marks the drafts published | `application_start_review_contract` | `refused`: "refusal-not-settled" |
| An unknown answer releases the drafts | `application_start_review_contract` | `uncertain`: "not-held" |
| Discard review discards the drafts | `application_start_review_contract` | `refused`: "discard-lost-the-draft" |
| No hold before the send | `application_start_review_contract` | `finish`: "not-submitted" (nothing to mark published) |
| No comment verification | `live_review_submission_provider` | "its comments are read once, after the POST" |
| A 422 is not definite | `live_review_submission_provider` | "GitHub's 422, with no new review of mine after it, is a definite refusal" |
| Withdrawal removes nothing | `sqlite_review_submission_recovery_storage` | "an unresolved attempt is withdrawn" |
| The codec forgets the mark | `application_comment_range_draft_store`; `application_start_review_contract` | "Start a review stores the draft pending, as version 3"; `finish`: "not-restored" |
| Storage admits a mark on another head | `application_comment_range_draft_store` | "storage refuses a mark on a head the draft was not written on" |
| An edit unmarks | `core_review_pending` | "a pending draft stays pending while it is edited" |
| A held draft released without its key | `core_review_pending` | "a held draft is released only with its review's answer" |
| Submit binds no comments | `core_review_pending` | "an edited pending comment refuses the Submit that showed the old one" |

**The 100 ms gates** (an optimized tests-on tree, `cmake --preset app-release
-B build/app-release-tests -DBUILD_TESTING=ON -DCMAKE_TEST_BUILD_DEPENDS=ON`,
Apple clang Release, run `-j1` at a load average near 9):
`application_large_review_responsiveness` passes with budgets enforced and no
`InputBudgetFailures` (keyboard line down 6.2 ms, extend 5.7 ms, reveal
7.7 ms), and `application_first_tab_comparison` passes.

**Pin.** Resealed over BPR-1365's reseal: `full_application` `ee58f1a4…`,
`production_app_manifest` `d0c0681a…`; the mutation pin is unchanged. The
dated note says what moved.

**Limits.**
- macOS only; Windows and Linux runs of these rows are owed by the stations.
- The comment verification reads one page of the pull request's comments: a
  pull request with more than 100 review comments leaves a review's outcome
  unknown until Check status can read it whole (not paged here).
- A pending draft belongs to the head it was written on. After a push the
  drafts of the earlier head are not shown, as before, and so are not in the
  review.
- The pending list in Discuss names each comment; it does not yet open it.

### Owner look v001 (answered)

One new short session, not the long default one. Four questions, each Accept
or Reject with any change in the note; none is self-certified.

- Session `7f4f63492b862fe14af5ea8e11ca26ce`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1345-start-a-review-v001`
  (repository root `/Users/jaeger/Documents/GitHub/BetterPullReview`), all
  four candidates ready at event cursor 12. The review host runs detached.
- Candidates, revision `frozen-3c806caaa-v001`: `BPR-1345-write-a-comment-v001`
  (AC1, fingerprint `3bb9205a…`), `BPR-1345-review-in-progress-v001` (AC2,
  `4c053a8a…`), `BPR-1345-finish-review-v001` (AC3, `547ee944…`) and
  `BPR-1345-discard-review-v001` (AC3's Discard review, `db2c99da…`).
- Frozen root `/Users/jaeger/.mask/attended-review/bpr1345-start-a-review-v001.tb6uhcch`:
  `3c806caaa` exported and built there (optimized, tests on) by
  `scripts/freeze_attended_build.py`; executable SHA-256 `d3e6a1e48840…`. Its
  own check printed `ATTENDED_FILE_SHOWN` for
  `src/ui/conversation_cards_view.cpp` on `inline-drawer-v1`, and the frozen
  executable passed the `refused` and `uncertain` courses offscreen.
- Launch: `python3 /Users/jaeger/.mask/attended-review/bpr1345-start-a-review-v001.tb6uhcch/launch.py`.
  Everything goes to the fixture server; nothing reaches GitHub.

Next: read the session's decisions. Accepts answer AC1 to AC3's owner halves
(AC5); a Reject comes back as v002.

## Owner verdicts v001 (2026-10-09)

Session `7f4f63492b862fe14af5ea8e11ca26ce` (state
`/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1345-start-a-review-v001`),
read from cursor 12, handed back at event 19. Where a candidate was answered
twice, the later decision stands.

- `BPR-1345-write-a-comment-v001` (AC1, fingerprint `3bb9205a…`): **Accept**,
  no note, decision `8af7409a-b3a6-4ded-898d-57707610ec92`, event 18.
- `BPR-1345-review-in-progress-v001` (AC2, fingerprint `4c053a8a…`):
  **Accept**, no note, decision `4a7fc8c1-4764-4ade-886f-c2d9ad1cfc81`,
  event 17.
- `BPR-1345-discard-review-v001` (AC3's Discard review, fingerprint
  `db2c99da…`): **Reject**, decision `619bcbd8-a594-4935-bafb-cb9bb3ab4dab`,
  event 14 (it revises event 13's note by adding its last paragraph). The
  owner's note, verbatim:

  > I don't like this button layout, stacked buttons is too much space
  >
  > These details on finish review should be hidden by default.
  >
  > Make this page beautiful, and remove some of this duplicate text and
  > reduce the wall of text feel.
  >
  > Discard review should delete all the review comments and should come with
  > a confirmation so it's not done accidentally. It should also close the
  > review page if confirmed.
  >
  > And fwiw `Review draft discarded.` comes up in the current flow after you
  > go to one of the draft comments that was from the original discarded
  > review and start a new review on it and try to finish that review.

  Attachments, kept in `/Users/jaeger/bpr-owner-att/`:
  `BPR-1345-discard-review-v001-0.png` (the drawer stacks "Add single
  comment" and "Add review comment" as full-width buttons),
  `BPR-1345-discard-review-v001-1.png` (the finish page's host, head and
  "Submitting as" lines) and `BPR-1345-discard-review-v001-2.png` (the finish
  page's repeated text: "Choose Comment, Approve, or Request changes" twice
  and "nothing is published until…" twice).
- `BPR-1345-finish-review-v001` (AC3, fingerprint `547ee944…`): **Reject**,
  decision `92d1924d-b7cf-4c14-a106-d33445bc5d20`, event 16 (it revises event
  15's note by adding its last sentence). The owner's note, verbatim:

  > We're lacking the ability to delete a comment.
  >
  > Start a review, add to review should be the default for cmd+enter.
  >
  > Do like cmd+shift+enter for single comment. And find some hotkey for
  > finish review, maybe cmd+option+enter
  >
  > I feel like these should be the action buttons that finish the review.
  > Not sure there's a good reason to have a separate submit review. If there
  > is a good reason to keep the 2 action flow then argue for it.

  Attachment `BPR-1345-finish-review-v001-0.png`: the Comment, Approve and
  Request changes buttons.

The Accepts answer AC1's and AC2's owner halves (AC5 for them). AC3 is v002's.

## v002 (bpr-main-mac, 2026-10-09)

Engineering owner `rfc-implementer` (`claude-opus-5-5`, effort high) on
`bpr-main-mac` (macOS, Apple clang, Qt 6.11.2, Debug `build/dev`,
offscreen). The `frontend-design:frontend-design` skill was loaded first.
Built in `/Users/jaeger/bpr-wt/bpr1345` on branch `bpr-1345-start-a-review`,
rebased onto `67ab0e6d3` (BPR-1360 and BPR-1369 landed during the round).

### Design

The finish page keeps the drawer's own visual language rather than adding a
new one: the drawer's 6 px pills, the palette's hairline for edges, the
highlight's tint under the pointer and its ring on focus. The one emphatic
thing on the page is the row of verdicts. No verdict is coloured, because on
this page colour would read as a judgement and nudge one choice over another.
The list and the summary share one hairline box family, so the page reads as
a single column, no wider than 760 px and left aligned. A file name that
repeats the row above is drawn faint. Line numbers use the highlight as ink
only where it reaches 4.5:1 on the text ground, and are otherwise moved toward
the text ink until they do (high-contrast themes). Words appear once: the
title, "N pending comments", the placeholder, the verdicts, "Nothing is
published until you submit." and Discard review. Everything else is in
tooltips, which the application anchors to their controls.

**Why there is no separate Submit review step.** The owner asked for an
argument if one exists. I found none strong enough to keep a second click on
every review. The two-step flow protects against a mis-click, but a verdict
button already names exactly what it does, its tooltip says so before the
press, and Approve keeps BPR-1085's amber dot and its "approving does not
resolve" tooltip while my blocking feedback is open. A `gh`-transport profile
still asks its CLI confirmation before any write. The pressed verdict is the
one submitted, and the prepared decision (BPR-1085) binds it. The question
text says this, so the owner can still reject it.

### What changed

1. **Drawer actions, one row** (`conversation_open_card`). The lock, Delete,
   then Add single comment (outlined) beside the main action (filled), all on
   one row. Where both pills do not fit, the main action keeps the row and a
   split arrow is joined to its right edge (`threadReviewDraftMore`); its menu
   carries Add single comment with its key. Nothing stacks.
2. **Keys, each bound once.** In the drawer draft's field ⌘↩ is the main
   action in every state (Start a review, Add review comment, or Update review
   comment), ⌘⇧↩ is Add single comment, and ⌘⌥↩ finishes the review.
   Maximize and its restore move from ⌘⇧↩ to ⌘⇧M. Finish review is also a
   window action (`finishReviewAction`, `Ctrl+Alt+Return` and keypad Enter,
   window context), enabled while a review is in progress and not being
   submitted. Each tooltip names its key, and the chip's tooltip begins
   "Finish review  ⌘⌥↩". A reply's ⌘↩ still publishes the reply. The window
   test counts ⌘⌥↩ bound once, and the existing collision test still passes.
3. **Delete a comment.** The drawer's trash is "Delete comment". When the
   draft has words it asks first ("Delete this comment?", or "… from your
   review?" for a pending draft; Cancel is the default and Esc), then deletes
   the draft from this computer. On Finish your review each row has its own
   Delete, which asks first and names the comment. Confirmed, the
   composition's `delete_pending_comment` discards that one draft at the
   revision the page listed; a draft changed since, or no longer pending, is
   left as it is.
4. **Finish your review** (`review_submission_editor`), as described in
   Design. Each verdict submits directly as itself, enabled only when the
   core's new `review_submission_controls::SubmitAs` says the review can be
   submitted so (`core::submits_as`). Request changes waits for a summary and
   says so; Comment waits for a summary or a line comment. A row goes to its
   comment (`workbench_window::reveal_pending_comment`: Review, its file, and
   the line selected once that file's comparison shows). "Pull request and
   account" is a disclosure, closed by default. The status line shows only
   outcomes, and Check status and New review show only when they can act. The
   workflow no longer sends guidance as status. `submit_action()` remains,
   unshown, for callers that submit the chosen verdict.
5. **Discard review deletes.** It asks first, naming the count ("Discard your
   review and its 2 pending comments?"). Confirmed, `discard_review_comments`
   discards every pending draft of the review in one store transaction (no
   longer clearing marks), and the page closes (`set_discard_observer`). A
   summary-only draft asks only when it has words.
6. **The "Review draft discarded." bug.** *Stale state:* the review
   submission workflow kept a terminal summary draft as the current one. v001
   page stayed open after Discard review and listed "Discard" for the fresh
   draft. Pressing it (or discarding any summary-only draft) left the
   workflow's draft `discarded` with no new draft, and Status "Review draft
   discarded.". `apply_review_comments` ignores comments while the draft is
   not drafting, so a later Start a review on an old draft was never carried.
   Finish review then showed "Review draft discarded.", "This draft was
   discarded" and a disabled Submit. A submitted review had the same
   shape. *Fix* (`review_submission_workflow`): every discard starts a new
   draft at once, unless an unfinished submission waits for New review. When
   pending comments arrive while the shown draft is submitted or discarded, a
   new draft starts for them. Discovery applies the same rule. The page never
   shows an earlier review's words again.

Also: the UIforETW review-bar order check expected 14 actions and now counts
the review chip that v001 put before Compose review (15). That check had
been red since v001. The threaded-reply course confirms the drawer's Delete
question.

### Verification (macOS, Debug, offscreen)

- `core_review_pending`: new `test_each_verdict_says_whether_it_can_submit`.
- `qt_review_submission_editor_scale_{100,125,200}`: the disclosure is closed
  by default and opens; the verdicts are buttons that submit; a keyboard press
  on the draft's verdict emits the exact submit with the decision shown;
  Request changes waits for a summary and its reason is anchored; the page
  says what is published once and an empty status takes no room; each row
  goes to its comment and its Delete asks first, Cancel deleting nothing; a
  page whose dispatch is the workflow's submits Approve and Request changes
  at once, each as itself; Discard review asks, naming the count, Cancel
  keeps, confirmed discards and asks the window to close; an empty draft is
  discarded without a question.
- `qt_conversation_cards_view`: ⌘↩ is Start a review, ⌘⇧↩ Add single
  comment, ⌘⌥↩ finish, ⌘⇧M maximize; tooltips name keys and are anchored;
  one row with room, split arrow joined and nothing cut off in a narrow
  drawer; the arrow's menu adds the single comment.
- `qt_workbench_line_draft`: Finish review's action, key and chip tooltip;
  ⌘⌥↩ bound once; ⌘⌥↩ through the window system opens Finish your review;
  disabled while submitting or with nothing pending. ⌘⇧M maximizes.
- `qt_workbench_conversation_reveal`: the drawer's Delete asks, Cancel keeps,
  Delete comment deletes; ⌘⇧↩ publishes once.
- `application_start_review_contract` (now five courses, the composed
  application over the fixture server): `pending`
  `START_REVIEW_PENDING_EVIDENCE:11111` (the third comment by ⌘⇧↩),
  `finish` `START_REVIEW_FINISH_EVIDENCE:11111` (the page as asked, submitted
  by the Comment verdict), `refused` `START_REVIEW_REFUSED_EVIDENCE:1111`
  (Discard review asks, deletes the draft and closes the page), `uncertain`
  `START_REVIEW_UNCERTAIN_EVIDENCE:1111`, and the owner's flow `again`
  `START_REVIEW_AGAIN_EVIDENCE:1111111111`. That course keeps an older draft
  out of the review, starts a review of two, opens Finish review by its
  action, presses a row (Review, its line selected) and comes back by the
  action, deletes one row, and discards the review, which deletes it, keeps
  the older draft and closes the page. It then discards a written summary on
  Compose review, takes the older draft into a new review by ⌘↩, and finishes
  it: the page lists it, says nothing of a discard, and Comment submits one
  review carrying exactly it.
- `application_review_submission_composition_public_path` (and scales 125,
  200), `application_threaded_reply_contract`,
  `application_uiforetw_selected_diff_contract`,
  `application_review_round_trip_composition_contract` (resealed),
  `application_composition_contract`.
- After rebasing onto `67ab0e6d3`, the 37 focused rows above (with
  `epoch_application_host` and the UIforETW contract) pass again at `-j6`.
- One broad pass before that rebase: 181 Qt-window, composed and application
  rows at `-j8`.
  Passing apart from the round-trip pin (resealed after) and the UIforETW
  order (fixed after). Two failures predate this card:
  `qt_workbench_home_discovery_scale_{100,125,200}` (noted on BPR-1365) and
  `application_native_notification_attended_contract`, which needs attended
  input.
- Renders of the composed application in light, dark, high-contrast light and
  high-contrast dark (drawer with Add review comment; Finish your review; the
  Delete and Discard questions), from the `again` course with
  `BPR_START_REVIEW_RENDER_DIRECTORY`.
- `coding-format` (clang-format 22.1.8) on every changed file;
  `DOCUMENTATION_OK`.

**Failing-before controls.** For each control, one mutation was applied, the
row built and run, and the file restored byte for byte. Every control failed.

| Mutation | Row | Failure |
|---|---|---|
| Every verdict follows the chosen one | `core_review_pending` | "with no summary and no line comment only Approve submits" |
| The drawer never splits | `qt_conversation_cards_view` | "in a narrow drawer: still one row, the main action with its split arrow joined to it, nothing stacked or cut off" |
| ⌘↩ publishes, as v001 | `qt_conversation_cards_view` | "⌘↩ is Start a review, the default …" |
| ⌘⇧↩ maximizes, as v001 | `qt_conversation_cards_view` | "in the draft, Ctrl+Shift+M maximizes, Ctrl+Shift+Return publishes it alone …" |
| Finish review has no key | `qt_workbench_line_draft` | "Finish review has its key, and the chip's tooltip names it" |
| A row's Delete asks nothing | `qt_review_submission_editor_scale_100` | "Delete asks first, naming the comment, with Cancel the default" |
| The drawer's Delete asks nothing | `qt_workbench_conversation_reveal` | "Delete asks first, and the draft stays open while it asks" |
| The composition deletes no row | `application_start_review_contract` | `again`: "row-not-deleted" |
| A row goes nowhere | `application_start_review_contract` | `again`: "row-not-revealed" |
| A verdict only chooses, as v001 | `qt_review_submission_editor_scale_100` | "keyboard press on the draft's verdict emits the exact submit intent" |
| The details show by default | `qt_review_submission_editor_scale_100` | "the target and account are behind a closed disclosure" |
| What is published is said with no draft and twice | `qt_review_submission_editor_scale_100` | "what is published is said once, and an empty status takes no room" |
| Discard review keeps the drafts, as v001 | `application_start_review_contract` | `refused`: "discard-kept-the-draft" |
| Discard review asks nothing | `qt_review_submission_editor_scale_100` | "Discard review asks first and names the count" |
| The page stays open after Discard review | `application_start_review_contract` | `refused`: "discard-kept-the-draft" (the page is still shown) |
| The stale discarded draft, as v001 | `application_start_review_contract` | `again`: "again-page-not-shown", the page saying "Review draft discarded." |

**The 100 ms gates** (`build/app-release-tests`, Apple clang Release, tests
on, run `-j1`), on the rebased candidate at a load average near 50 just after
its build: `application_large_review_responsiveness` passes with budgets
enforced, no `InputBudgetFailures` and no `UnenforcedLimitMisses` (largest
input interval 10.9 ms, p95 6.6 ms, largest wheel event 3.9 ms), and
`application_first_tab_comparison` passes. Before the rebase, at a load near
3.5: 11.0 ms, 7.0 ms and 4.2 ms.

**Pin.** Resealed over BPR-1369's reseal (BPR-1367 and BPR-1368 before it
left both pins matching) in `1cb430214`, and again for the testing-only row
check after it: `full_application` `30381942…`, `production_app_manifest`
`f5d1d596…`; the mutation pin is unchanged. The dated notes say what moved.

**Limits.**
- macOS only; Windows and Linux runs of these rows are owed by the stations.
- A pending row goes to its comment's line in Review; it does not open the
  drawer composer on it.
- Undo is not offered for Delete or Discard review; each asks first instead.
- The v001 session's host (pid 70880) and the frozen v001 application the
  owner launched (pid 17530) are not this round's processes and were left
  running; the v001 root is not removed.

### Owner look v002 (answered)

One new short session, not the long default one: four Accept/Reject questions,
one per item group. None is self-certified.

- Frozen root `/Users/jaeger/.mask/attended-review/bpr1345-start-a-review-v002.90kd9blu`:
  `1cb430214` exported and built there (optimized, tests on) by
  `scripts/freeze_attended_build.py`, executable SHA-256
  `9068fc6e95ada710729e2dc55c271a9f5290e24c0b714e05359af2bb7fe5345c`. Its own
  check printed `ATTENDED_FILE_SHOWN` for `src/ui/conversation_cards_view.cpp`
  on `inline-drawer-v1`. The frozen executable passed the `again` course
  offscreen (`START_REVIEW_AGAIN_EVIDENCE:111111111`, before the row check was
  added; the product code is the same). Launch:
  `python3 /Users/jaeger/.mask/attended-review/bpr1345-start-a-review-v002.90kd9blu/launch.py`.
  Everything goes to the fixture server; nothing reaches GitHub.
- Session `6b69681ee0f2c70e5c926cb85b3c72ae`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1345-start-a-review-v002/state`
  (repository root `/Users/jaeger/Documents/GitHub/BetterPullReview`). All four
  candidates were ready at event cursor 12; the host runs detached (pid 98607).
- Candidates, revision `frozen-1cb430214-v002`:
  `BPR-1345-drawer-actions-keys-v002` (`18a67dd9…`),
  `BPR-1345-delete-comment-v002` (`dd6f0162…`),
  `BPR-1345-finish-page-v002` (`5a46d88b…`; its text argues against keeping a
  separate Submit review step) and `BPR-1345-discard-review-v002`
  (`a088537e…`; it walks through the owner's flow behind "Review draft
  discarded."). Each carries renders from the composed application.

Next: on hand-back, read `status --after 12`. Accepts answer AC3's owner half
and the v002 groups (AC5); a Reject's note is the next round's change.

## Owner verdicts v002 (2026-10-09)

Session `6b69681ee0f2c70e5c926cb85b3c72ae` (state
`/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1345-start-a-review-v002/state`),
read from cursor 12, handed back at event 17 with nothing pending. Each
candidate was answered once.

- `BPR-1345-drawer-actions-keys-v002` (fingerprint `18a67dd9…`): **Accept**,
  no note, decision `97b6bfee-5d7c-450a-9c7d-40dc1d65af71`, event 15.
- `BPR-1345-delete-comment-v002` (fingerprint `dd6f0162…`): **Reject**,
  decision `3cdccbf0-fd3f-4351-b32e-c809129f8323`, event 13. The owner's
  note, verbatim:

  > Render the comment better. Put it in a box and start the comment below
  > the location record. Render it basically like it would appear in the
  > drawer.
  >
  > Make sure the comment is clearly separated from the dialog information.
  > Color the delete box red ane ensure it looks dangerous sort of like this
  > one from codex
  >
  > We don't need to say "it is deleted from this computer" The cannot be
  > undone warning is sufficine.t

  Attachments, kept in `/Users/jaeger/bpr-owner-att/`:
  `BPR-1345-delete-comment-v002-0.png` (the native message box: a warning
  triangle, the comment inline after its location, and "It is deleted from
  this computer. This can't be undone.") and
  `BPR-1345-delete-comment-v002-1.png` (the reference: a dark rounded panel,
  a bold "Delete task?", muted words ending "This cannot be undone.", a close
  ×, and a neutral Cancel beside Delete in red words on a dark red tint).
- `BPR-1345-discard-review-v002` (fingerprint `a088537e…`): **Reject**,
  decision `a8f82732-68c0-40eb-a24a-c73acbc1ffc4`, event 14. The owner's
  note, verbatim:

  > Style the discard button similar to the feedback I gave on delete comment.

- `BPR-1345-finish-page-v002` (fingerprint `5a46d88b…`): **Reject**, decision
  `de957e24-351c-4f71-9360-46af71807bc8`, event 16. The owner's note,
  verbatim:

  > I like everything but this new review page... once the review is
  > submitted I just want it to close and return to the PR instead.

  Attachment `BPR-1345-finish-page-v002-0.png`: after submitting, the page
  stays open with "This review was submitted. Choose New review to write
  another.", "Review submitted and confirmed on GitHub with its 2 line
  comments." and a New review button.

The Accept answers the drawer actions and keys (AC1's v002 change; AC5 for
it). The three Rejects are v003's. Cleanup after reading: the session's
candidates released (`resume-development --all`, event 18), its host stopped
(pid 98607, `stop`), and both frozen roots removed with
`scripts/freeze_attended_build.py remove`
(`bpr1345-start-a-review-v002.90kd9blu`, and `bpr1345-start-a-review-v001.tb6uhcch`,
from which no process was running).

## v003 (bpr-main-mac, 2026-10-09)

Engineering owner `rfc-implementer` (`claude-opus-5-5`, effort high) on
`bpr-main-mac` (macOS, Apple clang, Qt 6.11.2, Debug `build/dev`,
offscreen). The `frontend-design:frontend-design` skill was loaded first.
Built in `/Users/jaeger/bpr-wt/bpr1345` on branch `bpr-1345-start-a-review`,
rebased onto `3084597af`.

### Design

The owner named the look, so v003 follows the reference rather than
inventing one: a rounded panel, a bold question, a close ×, muted words, a
neutral Cancel and a red deleting button. Two choices are v003's own. The
panel is raised from the window and the comment's box is recessed back onto
the window's ground, behind a hairline. The comment therefore reads as a
quoted object apart from the question's words, and it sits as a drawer post
does: the label's mark in a left gutter, the recognized header in semibold,
the Markdown rendered in the drawer's reader and code font. Red appears in
one place only, the deleting button (and Discard review on the page), so it
keeps its meaning. Every colour derives from the palette of the window that
asks, because the themes are applied per window; v002's native box ignored
the dark theme.

### What changed

1. **One destructive question** (`destructive_question`, new in the Qt
   adapter). A frameless, rounded, window-modal panel painted in the theme's
   own colours: the question in bold, a close × (named "Close"), the muted
   words "This can't be undone.", then Cancel and the deleting button on one
   row at the right. Cancel is the default and takes the focus. Esc, Cancel
   and × cancel; only the deleting button confirms. The deleting button's
   words are red on a red tint in all four themes, at least 4.5:1 on that
   tint (7:1 in a high-contrast theme), and it is named for what it does
   ("Delete comment", "Discard review"). The question follows its window's
   theme while it is open. It replaces the three compose-flow message boxes:
   the drawer's Delete, a Finish your review row's Delete, and Discard
   review. ("Forget server" is not in the compose flow and is unchanged.)
2. **Delete comment.** Under the title, the location on its own line (the
   file in semibold, then "lines 3–4" muted), then the comment in its box as
   the drawer reads it, then "This can't be undone.". The words "deleted
   from this computer" are gone (owner: "The cannot be undone warning is
   sufficient").
3. **Discard review.** The same question, naming the count ("Discard your
   review and its 2 pending comments?", or "Discard your review summary?").
   The page's Discard review button (`danger_tool_button`) has red words and
   no fill at rest, and the red tint and edge under the pointer. It is a
   secondary danger and does not compete with the verdicts. Its words read at
   4.5:1 on the page in every theme.
4. **A confirmed review closes the page.** When GitHub confirms the review
   (on Submit, or later through Check status), the workflow starts the next
   review at once and tells the editor which review was confirmed
   (`confirm_submitted`). The window then closes Finish your review, returns
   to the view the reviewer came from (Review or Discuss), and shows "Review
   submitted with N comments" in the status bar for eight seconds, announced
   politely. The "This review was submitted. Choose New review…" text and the
   New review button are gone. Nothing waits for New review any more: a
   confirmation or a discard moves on to the next unfinished submission if one
   is waiting, else a new draft, and discovery replaces a finished draft. A
   refused or unknown answer keeps the page, with its words and Check status.

### Verification (macOS, Debug, offscreen)

- New `qt_destructive_question_scale_{100,200}`: the panel's structure (bold
  title, the window's title and accessible name; the words only "This can't
  be undone."; the location on its own line; the comment's box below it and
  above the words; the comment rendered with its mark in the left gutter and
  its header set apart; a named ×; Cancel then the deleting button on one
  row); Cancel the default and focused; Esc, Return on Cancel and × cancel,
  the deleting button confirms; in each of the four themes the question takes
  its window's palette, the deleting button is red words on a red tint at
  4.5:1 (7:1 high contrast), the muted words and title read on the panel,
  Cancel is neutral, the panel's edge shows, and Discard review's words are
  red and read on the page. It renders both questions in each theme.
- `qt_review_submission_editor_scale_{100,125,200}`: no New review button; a
  row's Delete shows the comment where it is, in its box, Cancel the default;
  Discard review asks in the shared question and its button is red; a
  confirmation is announced once with its count.
- `qt_workbench_window_scale_100` (`workbench_window_tests`): the drawer's
  Delete asks in the shared question with the draft's words in the box.
- `application_start_review_contract` (the composed application over the
  fixture server): `finish` now requires the page closed, Review shown and
  "Review submitted with 2 comments"; `refused` requires the page still open
  with "Review was not published."; `uncertain` requires the page open while
  the answer is unknown, and closed with "Review submitted with 1 comment"
  once Check status finds it; `again` requires the page closed after its
  submission. `application_review_submission_composition_public_path` (and
  scales 125 and 200): each confirmation is heard once, the window says
  "Review submitted…", and the next review is already shown.
- On the rebased candidate, 26 focused rows pass at `-j6`, including the
  above, `qt_workbench_line_draft`, `qt_workbench_conversation_reveal`,
  `qt_conversation_cards_view`, `qt_workbench_window_scale_{100,125,200}`,
  `application_threaded_reply_contract`,
  `application_uiforetw_selected_diff_contract`,
  `application_composition_contract`,
  `application_review_round_trip_composition_contract` (resealed),
  `core_review_pending`, `core_review_submission`, `architecture_boundaries`,
  `architecture_boundary_contract` and `test_network_isolation_manifest`.
- Renders of the composed application in light, dark, high-contrast light and
  high-contrast dark, from the `again` course with
  `BPR_START_REVIEW_RENDER_DIRECTORY`: both questions over the window, Finish
  your review with the red Discard review, and Review after the submission
  with its status line.
- The 100 ms gates were not run again: v003 changes nothing on the input or
  scroll path (a question panel, a status message, and the workflow's handling
  of an answer), so v002's pass stands.
- `coding-format` (clang-format 22.1.8) on every changed file;
  `DOCUMENTATION_OK`.

**Failing-before controls.** For each control, one mutation was applied, the
row built and run, and the file restored byte for byte. Every control failed.

| Mutation | Row | Failure |
|---|---|---|
| Delete's words say "It is deleted from this computer." again | `qt_review_submission_editor_scale_100` | "Delete asks first, showing the comment where it is, in its box, with Cancel the default" |
| No comment box (the subject is not set) | `qt_review_submission_editor_scale_100` | the same |
| The deleting button's words in the text colour | `qt_destructive_question_scale_100` | "the deleting button is red words on a red tint" |
| The deleting button takes the focus | `qt_destructive_question_scale_100` | "Cancel is the default and takes the focus" |
| The close × confirms | `qt_destructive_question_scale_100` | "the close mark cancels" |
| Discard review drawn in the text colour | `qt_review_submission_editor_scale_100` | "Discard review is drawn in the deleting red, readable on the page" |
| The page stays open on a confirmation | `application_start_review_contract` | `finish`: `START_REVIEW_FINISH_EVIDENCE:11110` (MarkedPublished) |
| A refusal closes the page | `application_start_review_contract` (`refused` run directly) | `START_REVIEW_REFUSED_EVIDENCE:1100` (RefusedKept) |
| The submitted review stays shown (no next review) | `application_review_submission_composition_public_path` | "the next review starts at once" |

During the `refused` control's ctest run, the `pending` course also timed out
once at an early phase at a load average near 140. Run directly under the
same mutation, `pending` passed (`START_REVIEW_PENDING_EVIDENCE:11111`). The
row passes on the candidate.

**Pin.** Resealed over the v002 follow-up reseal: `full_application`
`20fc8b08…`, `production_app_manifest` `043f3145…`. The mutation pin is
unchanged. The dated note says what moved.

**Limits.**
- macOS only; Windows and Linux runs of these rows are owed by the stations.
- The panel is a frameless top-level window. macOS draws its shadow; on
  Windows and Linux it has its hairline edge and no shadow.
- The status line is the window's status bar message; it goes after eight
  seconds or when another message replaces it.
- A confirmation that restores an earlier unfinished submission closes the
  page as well. That submission is shown, with Check status, the next time
  Compose review or Finish review opens.

### Owner look v003 (answered)

One new short session, not the long default one: three Accept/Reject
questions, one per rejected v002 item. None is self-certified.

- Frozen root `/Users/jaeger/.mask/attended-review/bpr1345-start-a-review-v003.i2hsl035`:
  `4f9d0f98b` exported and built there (optimized, tests on) by
  `scripts/freeze_attended_build.py`, executable SHA-256
  `121661a3079c5468878bcfb193a1509528a22abe9f40c7c4df3f960de228dbe0`. Its own
  check printed `ATTENDED_FILE_SHOWN` for `src/ui/conversation_cards_view.cpp`
  on `inline-drawer-v1`. The frozen executable passed the `again`
  (`START_REVIEW_AGAIN_EVIDENCE:1111111111`), `refused` (`1111`) and
  `uncertain` (`1111`) courses offscreen, and the `again` run made the
  session's renders. Launch:
  `python3 /Users/jaeger/.mask/attended-review/bpr1345-start-a-review-v003.i2hsl035/launch.py`.
  Everything goes to the fixture server; nothing reaches GitHub.
- Session `15cc2ae59c19c6c52ad63fbc46e73f35`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1345-start-a-review-v003/state`
  (repository root `/Users/jaeger/Documents/GitHub/BetterPullReview`). All
  three candidates were ready at event cursor 9; the host runs detached
  (pid 28600).
- Candidates, revision `frozen-4f9d0f98b-v003`:
  `BPR-1345-delete-comment-v003` (`5ac9bbaa…`),
  `BPR-1345-discard-review-v003` (`da743d34…`) and
  `BPR-1345-finish-closes-v003` (`01d868ac…`). Each carries renders from the
  frozen application; the delete question also carries the unit render of a
  labelled comment, its mark in the gutter.

Read on hand-back below.

## Owner verdicts v003 (2026-10-09)

Session `15cc2ae59c19c6c52ad63fbc46e73f35` (state
`/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1345-start-a-review-v003/state`),
read from cursor 9, handed back at event 14 with nothing pending. All three
are Accepts on revision `frozen-4f9d0f98b-v003`.

- `BPR-1345-delete-comment-v003` (fingerprint `5ac9bbaa…`): **Accept**,
  decision `81a1f1f7-bddc-465b-88a0-8b0b4d45f700`, event 11. It revises
  event 10's Accept (decision `38c3f83f-0aaf-43cc-af67-99e2745fbb6c`, note
  "This is good, but please say cannot instead of can't") by adding the note's
  last clause. The owner's note, verbatim:

  > This is good, but please say cannot instead of can't, no need for a
  > follow-up review for that change.

- `BPR-1345-discard-review-v003` (fingerprint `da743d34…`): **Accept**, no
  note, decision `93a50f80-df15-43f8-9342-8cb019d1d6f5`, event 12.
- `BPR-1345-finish-closes-v003` (fingerprint `01d868ac…`): **Accept**, no
  note, decision `2a220b19-bcb6-4cfa-89d8-b339cd3f5141`, event 13.

These answer AC3's owner half (Finish your review, Discard review and the
page closing on a confirmed review) and the last of AC5. With v001's Accepts
of write-a-comment and review-in-progress (AC1, AC2) and v002's Accept of
the drawer actions and keys, every question asked of the owner is accepted.

### The wording change (bpr-main-mac, 2026-10-09)

In `4ec36997e`, the shared question's words are "This cannot be undone." in
all three places the compose flow asks (the drawer's Delete, a Finish your
review row's Delete, Discard review), and in the `destructive_question`
documentation. The Qt rows check the exact words. The owner said no
follow-up review is needed for it, so none was asked. The workbench
specification now describes the shared question, Discard review's red button
and the page closing on a confirmed review, and no longer mentions New
review. No `src/app` source changed, so both round-trip pins still match.

On macOS (Debug `build/dev`, offscreen, `-j6`), 13 of 13 pass:
`qt_destructive_question_scale_{100,200}`,
`qt_review_submission_editor_scale_{100,125,200}`,
`qt_workbench_conversation_reveal`, `qt_conversation_cards_view`,
`qt_workbench_line_draft`, `qt_workbench_window_scale_{100,125,200}`,
`application_start_review_contract` and
`application_review_round_trip_composition_contract`. `coding-format`
(clang-format 22.1.8) leaves every changed file as it is. `DOCUMENTATION_OK`.

**Failing-before controls.** Each production string was put back to "can't"
and the row built and run, then the file restored byte for byte:

| Mutation | Row | Failure |
|---|---|---|
| A row's Delete and Discard review say "can't" | `qt_review_submission_editor_scale_100` | fails |
| The drawer's Delete says "can't" | `qt_workbench_conversation_reveal` | "Delete asks first, and the draft stays open while it asks" |

Correction to v003's verification: the drawer's Delete is checked by
`qt_workbench_conversation_reveal` (`--conversation-reveal`), not by
`qt_workbench_window_scale_100`, which passed under the second mutation.

## Closeout (bpr-main-mac, 2026-10-09)

- AC1: drawer foot, pending mark and restart: built in v001 and v002, with
  `application_start_review_contract`'s `pending` and `finish` courses. The
  owner accepted write-a-comment v001 and drawer-actions-keys v002.
- AC2: the review chip with its exact count and every pending draft drawn at
  its line: owner Accept of review-in-progress v001, plus
  `qt_workbench_line_draft` and the `pending` course.
- AC3: one review carrying exactly the pending drafts, the summary rule,
  publication only on GitHub's acceptance, and a refusal or unknown answer
  keeping every draft. The `finish`, `refused`, `uncertain` and `again`
  courses check these, along with `live_review_submission_provider` and
  `github_mutation_contract`. Discard review with its confirmation (v002,
  v003) and the verdicts submitting directly (v002): the owner accepted
  discard-review v003 and finish-closes v003.
- AC4: the composed public path through the fixture server, the
  failing-before controls (v001, v002, v003 and the wording change), the
  100 ms gates (v002; nothing on the input path changed since), the
  round-trip pin resealed (v003, matching at `4ec36997e`), and one live
  probe on `MikeMarcin/BPR-Test#44` (v001).
- AC5: one Accept/Reject question per criterion in three short sessions
  (`7f4f6349`, `6b69681e`, `15cc2ae5`). Every Reject was rebuilt and then
  accepted.

The card finishes on the coordinator's direction. These limits remain:
- Windows and Linux runs of this card's rows are not recorded and are owed by
  the stations.
- A submission's comments are verified from one page of the pull request's
  review comments, so on a pull request with more than 100 the outcome stays
  unknown; the read is not paged (v001's limit).
- A pending draft belongs to the head it was written on.

Cleanup: the session's candidates were released, its host (pid 28600) was
stopped, and the frozen root
`/Users/jaeger/.mask/attended-review/bpr1345-start-a-review-v003.i2hsl035`
was removed with `scripts/freeze_attended_build.py remove`.
