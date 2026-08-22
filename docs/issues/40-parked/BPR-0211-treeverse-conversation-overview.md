---
id: BPR-0211
title: A secondary overview for conversations too large to follow inline
status: Parked
rfc: RFC-BPR-0005
depends_on: [BPR-0191, BPR-0413]
parked_on: awaiting-product-decision
---

# A secondary overview for conversations too large to follow inline

> **Scope widened, 2026-08-08.** The owner narrowed the drawer to the *current
> file's* line-anchored threads, SmartBear Collaborator style, and called a
> whole-review conversation view "might be nice" rather than what they typically
> want. So this card now owns two related secondary views and not one: the
> whole-review conversation, and a Treeverse-style overview of a thread too deep
> to follow. Both are secondary to the anchored drawer, and both stay deferred.

Deferred deliberately, on the product owner's instruction, 2026-08-08.

The conversations drawer will draw its threads in a git-graph style with
filtering by lane ([[BPR-0191]]). That is expected to be enough for the
conversations a review normally carries.

The owner named the case it might not cover: a conversation unwieldy enough that
following a reply chain inline stops working. Their reference is Treeverse, which
they reach for on Twitter threads precisely because the linear presentation
becomes confusing at depth — *"great for linear conversation but confusing enough
that I had often reached for it"*.

## Why this is deferred rather than declined

It is a second view of the same data, and a second view is only worth building
once the first one has been used and found wanting. Building both now would mean
guessing which one reviewers actually reach for, and the guess would be made by
the person who wrote them rather than by anyone reviewing code.

The trigger is evidence, not size: a reviewer saying they lost their place, or a
recorded conversation whose depth makes the lane view unreadable at a supported
window width.

## What would need to be true first

- The lane view exists and has been used against a real recorded conversation —
  [[BPR-0187]] records `microsoft/DirectXMath#336`, which has two commits and no
  conversation at all, so it is not the fixture that answers this.
- A conversation deep or wide enough to be a problem is recorded, so the
  question is asked against real content rather than synthesised depth.

## Scope note, so this is not misread later

This is a *presentation* card. Nothing here changes what a conversation is, how
it is fetched, or what RFC-BPR-0002 owns. The data is already a forest of trees
with a single optional parent per node; both views read the same projection.

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

So this card is parked again, and this time the label is accurate: the owner is
the only thing left. Nothing is queued in front of it.

One finding from building them belongs on this card, because it changes what
the choice costs. The two alternatives are not equally expensive. The
whole-review view needs no model change. The Treeverse map needs one line:
`comment.ReplyTo` is fetched and validated, and
`github_projection_mapping.cpp:175` writes the thread root over it, which is
why today's tree is two levels deep. And no recorded pull request here carries
a conversation at all, so the deep thread was drawn synthesized — the
precondition this card set, that a conversation deep enough to be a problem is
recorded, is measured now rather than merely suspected, and it is still unmet.
