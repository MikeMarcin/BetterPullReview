---
id: BPR-0212
title: Show conversation and code evolving together across revisions
status: Done
rfc: none
depends_on: [BPR-0191, BPR-0413, BPR-0415]
---

# Show conversation and code evolving together across revisions

Product owner, 2026-08-08:

> There is a thing that IMO existing github is extra terrible at and maybe we
> can do better which is seeing the evolution of conversations and code together
> as PRs get new revisions

**This is RFC-scale and has no RFC.** It is recorded here so the idea and the
measurement behind it survive; it is not an issue slice and should not be worked
as one.

## Why the complaint is well founded

GitHub collapses a comment's history to one boolean. When a branch is pushed,
a review comment whose anchor moved becomes *outdated*, the diff it referred to
is folded away, and the reader is left with a remark about code they can no
longer see next to code nobody remarked on. The conversation and the code both
have a history and the interface shows neither.

## What this project already has, which is most of it

Two axes exist and have never been joined.

**The revision axis is real and retained.** `accepted_pull_request_revision`
carries `Base` and `Head` OIDs and a `pull_request_revision_origin` of live or
recorded, and RFC-BPR-0003's epoch catalog keeps arbitrary retained epochs.
`epoch_comparison_control` already offers *independent left and right selectors
over arbitrary retained epochs*, so a reviewer can already compare any two
revisions of the code. That is the hard part and it is done.

**The conversation axis is a forest of trees**, one optional parent per node,
with `Path` and `Line` anchors — see [[BPR-0191]].

## The gap, exactly

`github_conversation_projection` carries `Outdated` and `Resolved` — both
booleans GitHub computed — and **no revision identity at all**. Not a commit
OID, not an epoch, nothing. A comment does not know which revision of the code
it was written against.

So the join cannot be made today, not because the views are missing but because
the datum is. That is the whole finding: this is a *model* gap first and a
presentation feature second, and building the presentation first would mean
inventing the anchor in the view.

## What it would enable, once a comment knows its revision

For a chosen left/right epoch pair, all of which the product can already
resolve:

- which comments were live on each side;
- which went outdated *between* those two revisions, and what the code did in
  that same interval — the two facts side by side, which is the thing being
  asked for;
- a comment shown against the code it was actually written about, because the
  revision it was made against is retained rather than folded away.

`Outdated` stops being a dead end and becomes a question with an answer.

## Why this needs an RFC rather than a card

It changes what a conversation *is* in the model, which is RFC-BPR-0002's
territory, and it consumes the epoch axis, which is RFC-BPR-0003's. It also
implies a fetch that records the revision a comment was made against, and
whether that is available unauthenticated from the API has not been checked.

Do not add a revision field to the projection as a side effect of a
presentation card. The two owners have to agree on it first.

## Not this card

[[BPR-0211]] is a secondary *view* of a conversation that is hard to follow.
This is a different thing: the conversation is not hard to follow, it is missing
the axis that would make it make sense. Neither substitutes for the other.

## Operator direction — mockups first — 2026-08-20

Recorded verbatim:

> I would like to see mockups before deciding.

This is a request for work, not a deferral. The card asked for a decision with
nothing to decide against, which was the honest position while there was nothing
to look at — but "build it, don't build it, or build the other one" is a hard
question to answer in prose about an interface.

So the disposition changes from *waiting on a decision* to *waiting on a
deliverable that this session owes*. What is needed is a rendered mockup of each
alternative, presented as one choose-exactly-one question, in the repository's own
offline attended review page rather than any other surface — the owner stated that
preference in the same exchange.

The mockups are not an implementation and must not be mistaken for one. They are
drawn against the real conversation model this product already has, so that what
is being judged is a design this codebase could actually reach, not an
illustration of a design in general.

That direction originally ended by keeping `parked_on: awaiting-product-decision`,
reasoning that the decision is still what this card is for. The lane rule does not
work that way. `40-parked/` is a frontier-root lane — a card sits there when a
person, a live service, or a product decision is the only thing left — and the
verifier fails a parked card that still has an unmet dependency. While a session
owes a deliverable, this card is waiting on another card, and that lane is
`10-backlog/`.

[[BPR-0413]] now owns that deliverable. This card returns to `40-parked/` as
`awaiting-product-decision` the moment the page exists, because that is when the
label becomes true.

## The mockups exist — 2026-08-21

[[BPR-0413]] built them. The page is
`docs/verification/reviews/conversation-view-mockups-v001/offline-package/index.html`,
batch `bpr:/attended-review/betterpullreview/40a5fd691f86739e55c6860a`, and it
presents the alternatives as one choose-exactly-one question. Rejecting every
alternative is a recordable answer and means build neither.

That page was held back rather than presented: an independent review falsified
its central cost claim before the owner saw it, so [[BPR-0413]] reopened and
corrected the batch. The corrected page is
`bpr:/attended-review/betterpullreview/c285f6948ed054de1342cfb1`, and it carries
a fourth subject the first build omitted — the git-graph drawer [[BPR-0191]]
recorded the owner asking for and never built.

So this card is parked, and the label is accurate: the owner is the only thing
left. Nothing is queued in front of it.

One finding from building them belongs on this card, because it changes what
the choice costs. The two alternatives are not equally expensive. The
whole-review view needs no model change. The Treeverse map needs a real change,
not the one line an earlier draft of this note claimed: `comment.ReplyTo` is
fetched and validated and then overwritten by the thread root at
`github_projection_mapping.cpp:175`, but the Qt model also refuses any parent
that is not a `review_thread` (`github_conversation_model.cpp:317-326`), so
changing the mapping alone would flatten the drawer to one level instead of
deepening it. And no recorded pull request here carries
a conversation at all, so the deep thread was drawn synthesized — the
precondition this card set, that a conversation deep enough to be a problem is
recorded, is measured now rather than merely suspected, and it is still unmet.

## Decided — 2026-08-21

The owner answered the [[BPR-0413]] page. Verdict
`bpr:/attended-review/betterpullreview/c285f6948ed054de1342cfb1`, validated at
`docs/verification/reviews/conversation-view-mockups-v001/verdict-validation.json`.

**Accepted: the join is worth an RFC.** That is the whole of what was
authorized, and this card's own warning is now the load-bearing one — accepting
the subject authorized an RFC, not a projection field. Nobody may add a revision
to `github_conversation_projection` as a side effect of a presentation card.

This card stays what it has always been: the recorded idea and the measurement
behind it, not an issue slice. [[BPR-0415]] owns writing the RFC, so this card
waits in `10-backlog/` behind it rather than being worked directly.

## What the page corrected about this card

The batch's first build told the owner that whether GitHub exposes the revision
a comment was made against "has not been checked — unauthenticated or at all".
An independent review falsified that from this repository's own code before the
page was presented, and the correction belongs here because it changes how
expensive the idea is:

- `src/application/include/bpr/application/github_read_model.hpp:223` carries
  `std::optional<commit_oid> Commit` per review, and
  `src/adapters/github/src/github_read_contract.cpp:1214-1241` **requires**
  `commit_id` on every review object, rejecting a response without it.
- `src/application/src/github_projection_mapping.cpp:145-156` then drops it —
  the same discard this card documents for the conversation nodes.
- The write path already models a comment written against a revision:
  `src/adapters/github/src/github_mutation_contract.cpp:117-122` sends
  `commit_id`, the exact anchor commit, and rejects a mismatched echo.

So a review-level join is nearer than "the datum exists nowhere" implied. What
is genuinely missing is narrower and still real: the review-thread GraphQL
selection asks for `id bodyText path line originalLine outdated author replyTo`
and does not ask for a comment's commit at all. Whether that field is available
unauthenticated remains unchecked, and the RFC has to answer it.

## Superseded by RFC-BPR-0021 — 2026-08-22

This card existed to keep an idea and its measurement alive while it had no RFC:
"it is not an issue slice and should not be worked as one." [[BPR-0415]] has now
written that RFC, and
`docs/rfcs/future/RFC-BPR-0021-conversation-revision-identity.md` carries
everything this card was holding — the owner's complaint verbatim, the two axes
that already exist, the datum that does not, and the warning against adding a
revision field as a side effect of a presentation card, which is now a
prohibited design rather than a note on a card.

Two things this card did not know are in the RFC as well: `Line` silently means
`originalLine` once a thread is outdated, and there is no `Side` discriminator,
so a comment on a deleted line cannot be expressed at all today.

Done because its purpose is discharged, not because the work is finished. The
work is RFC-BPR-0021's, and that RFC is `Deferred` behind RFC-BPR-0002 and
RFC-BPR-0003 plus probe evidence that a real service returns a per-comment
commit.
