---
schema: "implement-rfc.card.v1"
id: "BPR-1345"
title: "Start a review: pending drafts that the review submission sends as review comments"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"BPR-1212 owner look, session aae8f3fcd307cfca3bfb8ae0b28f1b83, start-a-review-v001 Accept","decision_id":"dc7e0a01-68f0-42b2-8d34-ede92665e96c","reservation":"docs/issues/_reservations/BPR-1345--BPR-1345--530408819266.json","design":"docs/design/BPR-1212-start-a-review-v1/index.html","related_cards":["BPR-1212","BPR-1085","BPR-1329","BPR-1294"],"related_rfcs":["RFC-BPR-0037","RFC-BPR-0052"],"timing":"Build after BPR-1212's right-click v002 slice (coordinator, 2026-10-08)"}
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
  an uncertain answer keeps every draft and its text. Discard review clears
  the pending marks and keeps the drafts.
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
Pending drafts are notes at their lines named "Pending in your review". The
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
