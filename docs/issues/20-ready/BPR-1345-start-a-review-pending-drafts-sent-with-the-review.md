---
schema: "implement-rfc.card.v1"
id: "BPR-1345"
title: "Start a review: pending drafts that the review submission sends as review comments"
status: "Ready"
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
