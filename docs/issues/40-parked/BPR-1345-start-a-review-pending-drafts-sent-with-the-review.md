---
schema: "implement-rfc.card.v1"
id: "BPR-1345"
title: "Start a review: pending drafts that the review submission sends as review comments"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"BPR-1212 owner look, session aae8f3fcd307cfca3bfb8ae0b28f1b83, start-a-review-v001 Accept","decision_id":"dc7e0a01-68f0-42b2-8d34-ede92665e96c","reservation":"docs/issues/_reservations/BPR-1345--BPR-1345--530408819266.json","design":"docs/design/BPR-1212-start-a-review-v1/index.html","related_cards":["BPR-1212","BPR-1085","BPR-1329","BPR-1294"],"related_rfcs":["RFC-BPR-0037","RFC-BPR-0052"],"timing":"Build after BPR-1212's right-click v002 slice (coordinator, 2026-10-08)"}
parked_on: "human-review"
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
  `START_REVIEW_AGAIN_EVIDENCE:111111111`. That course keeps an older draft
  out of the review, starts a review of two, opens Finish review by its
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
left both pins matching): `full_application` `8f04d2ef…`,
`production_app_manifest` `edbe131f…`; the mutation pin is unchanged. The
dated note says what moved.

**Limits.**
- macOS only; Windows and Linux runs of these rows are owed by the stations.
- A pending row goes to its comment's line in Review; it does not open the
  drawer composer on it.
- Undo is not offered for Delete or Discard review; each asks first instead.
- The v001 session's host (pid 70880) and the frozen v001 application the
  owner launched (pid 17530) are not this round's processes and were left
  running; the v001 root is not removed.
