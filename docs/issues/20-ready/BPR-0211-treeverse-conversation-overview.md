---
id: BPR-0211
title: Build the Treeverse-style overview of a deep thread
status: Ready
rfc: RFC-BPR-0005
depends_on: [BPR-0191, BPR-0413, BPR-0416, BPR-0417, BPR-0418]
---

# Build the Treeverse-style overview of a deep thread

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

**Alternative B — the Treeverse-style map — is chosen.** The whole-review view
is recorded as *not chosen*, which is not a rejection: it was the losing side of
one question, and if B is later withdrawn this card does not have to
re-litigate whether A was defective. It was not.

The owner's words, verbatim:

> I prefer this view, it still needs to show the full branch that you clicked
> on, or at least up to the noce you selected. I think this could use some more
> iteration but that can come after experience.

So this card stops being "should a second view exist" and becomes "build this
one". Two things follow directly.

**The map must show the branch, not just the node.** The mockup highlighted the
root-to-focus path and put the focused reply in a side pane; the owner wants the
full branch that was clicked visible, or at minimum everything from the root
down to the selected node. Treat the highlighted path in
`mockups/03-treeverse.svg` as the floor, not the design.

**Iteration is expected and is not a defect.** "This could use some more
iteration but that can come after experience" is explicit permission to ship a
first version and learn from it, and an instruction not to hold the card open
chasing a final shape before anyone has used it.

## What it actually costs

Corrected during [[BPR-0413]], after an independent review falsified the first
estimate. This is not a one-line change:

- `src/application/src/github_projection_mapping.cpp:175` — carry
  `comment.ReplyTo` instead of the thread root, with a fallback for a thread's
  first comment, whose `replyTo` is null.
- `src/adapters/qt/src/github_conversation_model.cpp:317-326` — stop rejecting a
  parent whose `Kind` is `review_comment`. **Without this the mapping change
  makes things worse**: every reply is rejected, promoted to a root, and the
  drawer flattens from two levels to one.
- The map surface itself, its focus pane, keyboard navigation, and
  accessibility.
- Per-comment read state, which nothing currently carries — the ledger's unread
  notion is a per-conversation cursor.
- The tests that encode today's two-level shape, in the application host and the
  Qt panel.

## Known before starting

No recorded pull request in this repository carries a conversation, and the
deepest real structure in any fixture is one `replyTo` edge. This card's own
earlier precondition — that a conversation deep enough to be a problem is
recorded — is still unmet, and the owner chose to proceed anyway. A first slice
should expect to build its own fixture with real depth, and should not claim
evidence from synthesized content that it has not labelled as such.

## Not this card

The owner's other remark on the same verdict — that the existing drawer needs to
anchor the root comment to the line of code it comments on — is a gap in the
shipped anchored drawer, not in this secondary view. [[BPR-0414]] owns it.

## Broken into slices — 2026-08-21

This card is now the umbrella and closes when its slices do. The required final
state above is a view plus a cross-layer data change plus content that does not
exist yet, which is three different kinds of work with three different ways of
failing.

- [[BPR-0416]] — carry the reply edge into the tree. The data foundation, and
  the half an independent review found missing from the original cost.
- [[BPR-0417]] — record a conversation with real depth. This card's own unmet
  precondition, now carded rather than restated.
- [[BPR-0418]] — build the map surface, including the owner's requirement that
  the full clicked branch be visible.

## Every slice has landed — 2026-08-25

[[BPR-0416]] carried the reply edge, [[BPR-0417]] recorded a conversation with
real depth, and [[BPR-0418]] built the map surface with the owner's full-branch
requirement met. This umbrella now owns only its own closing record: what
shipped, what was recorded *not chosen*, and what the first version deliberately
left for iteration after use.
