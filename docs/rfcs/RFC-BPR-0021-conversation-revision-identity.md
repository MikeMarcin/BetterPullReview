---
id: "RFC-BPR-0021"
title: "Conversation Revision Identity and the Comment-Code History Join"
status: "Deferred"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0002","RFC-BPR-0003"]
pack_version: "post-v2"
---

# RFC-BPR-0021: Conversation Revision Identity and the Comment-Code History Join

## Status and scope

This is a deferred post-pack RFC. It does not alter the canonical fourteen-RFC
sequence and blocks no pack-v2 implementation or closeout.

It exists because the product owner asked for it. The complaint, recorded on
2026-08-08:

> There is a thing that IMO existing github is extra terrible at and maybe we
> can do better which is seeing the evolution of conversations and code together
> as PRs get new revisions

That idea was held in [[BPR-0212]] without an RFC for two weeks, because it
changes what a conversation *is* in the model and no card may make that change
as a side effect of a presentation slice. On 2026-08-21 the owner reviewed a
rendered mockup of the join and accepted the subject. That acceptance authorized
exactly one thing: writing this document.

Nothing here is implemented. No field has been added to any projection.

## Problem

GitHub collapses a comment's history to one boolean. When a branch is pushed, a
review comment whose anchor moved becomes *outdated*, the diff it referred to is
folded away, and the reader is left with a remark about code they can no longer
see, next to code nobody remarked on. The conversation has a history and the
code has a history, and the interface shows neither.

BetterPullReview is unusually well placed to fix this and currently does not,
for one specific reason: a comment does not know which revision of the code it
was written against.

## Findings established from the codebase

Each was read from live source rather than inferred, during [[BPR-0413]] and
[[BPR-0415]].

**The revision axis exists and is strong.** `accepted_pull_request_revision`
(`src/core/include/bpr/core/epoch_model.hpp:382-392`) carries `Base` and `Head`
commit OIDs and an `Origin` of `live` or `explicit_historical_import`.
`review_epoch_summary` retains arbitrary observed epochs with their cause and
availability. `epoch_comparison_control`
(`src/adapters/qt/include/bpr/qt/epoch_comparison_control.hpp:30-127`) already
offers independent left and right selectors over those retained epochs, plus
free-OID entry and the named comparisons. A reviewer can already compare any two
revisions of the code; that is the hard part and it is done.

**The conversation axis exists.** `github_conversation_projection`
(`src/application/include/bpr/application/github_projection.hpp:124-139`) is a
forest with one optional parent per node, carrying `Path`, `Line`, `Resolved`,
`Outdated`, and `Deleted`. Since [[BPR-0416]] a reply nests under the comment it
replied to rather than under the thread root.

**The projection carries no revision identity at all.** Verified field by field
at the same location: no commit OID, no epoch, no ordinal. `Outdated` is a
boolean GitHub computed, and nothing behind it says relative to what.

**A review's commit is already fetched, required, and then discarded.**
`github_review_record` carries `std::optional<commit_oid> Commit`
(`src/application/include/bpr/application/github_read_model.hpp:223`), and
`src/adapters/github/src/github_read_contract.cpp:1214-1241` **requires**
`commit_id` on every review object and rejects a response without it.
`src/application/src/github_projection_mapping.cpp:145-156` then drops it when
it builds the `review` node. So a review-level join is nearer than the original
framing of this problem suggested.

**The write path already models a comment against a revision.**
`src/adapters/github/src/github_mutation_contract.cpp:117-122` sends `commit_id`
— the exact anchor commit — when BetterPullReview creates a review comment, and
`:804,831` parses the echoed value back and rejects a mismatch.

**`Line` silently means two different things.**
`src/adapters/github/src/github_read_normalizer.cpp:156-165` computes
`selected = aPreferred.isNull() ? aFallback : aPreferred` and is called as
`optional_line(thread["line"], thread["originalLine"])`. GitHub nulls `line`
once a thread is outdated, so an outdated thread's `Line` is an `originalLine`
in a revision the pane may not be showing, wearing the same field name as a live
thread's current-head line.

**There is no `Side`.** Nothing on the projection says whether a line is on the
base or the head, so with two panes there is no way to know which axis a comment
belongs to. A comment on a deleted line is base-side only and cannot be
expressed today.

## Findings requiring direct probe evidence

One question is open and this RFC must not assume its answer.

The review-thread GraphQL selection in
`src/adapters/github/src/github_read_contract.cpp:33-37,558-566` requests
`id bodyText path line originalLine outdated author replyTo` for every comment.
It does **not** request the comment's commit. GitHub's schema exposes
`originalCommit` and `commit` on a review comment, but this project has not
probed either, and specifically has not established:

1. whether the per-comment commit is returned for the review-thread selection
   this product actually issues;
2. whether it is available to an **unauthenticated** read, which matters because
   RFC-BPR-0017 wants the same data without a credential;
3. whether GitHub Enterprise Server 3.18 returns it identically, which the
   fixture-security contract requires of every recorded response.

Until those are probed against a real service, this RFC records the join as
*designed and unproven* rather than available.

## What the join delivers

For a chosen left/right epoch pair, all of which the product can already
resolve:

- which comments were live on each side;
- which went outdated *between* those two revisions, and what the code did in
  that same interval — the two facts side by side, which is the thing the owner
  asked for;
- a comment shown against the code it was actually written about, because the
  revision it was made against is retained rather than folded away.

`Outdated` stops being a dead end and becomes a question with an answer.

## Recommendation

Add revision identity to the conversation model, at the read boundary, and
project it. Specifically:

1. Extend the read model's review-comment record with the comment's commit, and
   request it in the review-thread selection. Keep it optional: a host that does
   not return it must degrade to today's behaviour rather than fail the read.
2. Carry it onto `github_conversation_projection` as an explicit optional
   revision identity, alongside a `Side` discriminator, so a comment can be
   placed on the correct pane and against the correct revision.
3. Resolve that identity against the retained epoch catalog, so a comment can
   name the epoch it belongs to rather than a bare OID the reviewer must
   recognise.
4. Present the join in the workbench as a comparison-scoped view of the
   conversation, respecting `RFC-BPR-0005:58` — comments remain in the drawer
   and never become inline full-width rows in the code view.
5. Migrate persisted conversations that predate the field as *unknown revision*,
   never as a guess. An inferred anchor is worse than an absent one.

## Prohibited designs

- Adding a revision field to the projection as a side effect of a presentation
  card. Two RFCs own the surfaces this crosses and both must agree first.
- Inferring the revision a comment was written against from `Line`,
  `originalLine`, or `Outdated`. Those are the symptoms of the missing datum,
  not a substitute for it.
- Treating an outdated comment's `Line` as a current-head line. It is not, and
  the normalizer's fallback is why.
- Claiming the join works on evidence from a fixture whose per-comment commit
  was authored by this project rather than returned by a real service.

## Acceptance criteria

1. A review comment read from GitHub carries the revision it was written
   against, or an explicit absence, through the read model and the projection.
2. A conversation node carries an explicit `Side`, so every placeable comment
   names the pane it belongs to and unplaceable ones are visibly unplaceable.
3. For any two retained epochs the product can select, the workbench can state
   which comments were live on each side and which went outdated between them.
4. A comment is displayed against the code of the revision it was written
   against, retained rather than folded away.
5. Conversations persisted before this change load as unknown-revision and are
   never silently assigned one.
6. A host that does not return the datum degrades to today's behaviour with a
   typed diagnostic, and no read fails because of it.
7. GitHub.com and GHES 3.18 are covered by recorded contract responses that are
   byte-identical where the fixture-security contract requires it.

## Verification matrix

| Claim | Direct surface | Command or interaction | What proves it |
|---|---|---|---|
| the datum is returned by a real service | live GitHub read | a designated probe against a public pull request | a recorded response carrying the per-comment commit, for both hosts |
| the datum reaches the projection | `project_github_pull_request` | focused application-host test over a recorded snapshot | a projected comment names its revision, and an absent one is `nullopt` rather than defaulted |
| epochs and comments join | the workbench comparison surface | select two retained epochs and read the conversation state | live/outdated per side matches the recorded epoch pair, against known fixture data |
| an unplaceable comment says so | the drawer and the panes | a comment with no `Side` or unknown revision | it is visibly not placed, with a stated reason, and no row is guessed onto a line |
| migration does not invent history | the persisted ledger | open a store written before the change | every pre-existing conversation reads as unknown revision |
| a host without the field still works | recorded contract fixtures | a fixture whose comments omit the commit | the read succeeds with a typed diagnostic and today's behaviour |

## Documentation impact

The conversation model is documented as part of RFC-BPR-0002's surface and the
epoch axis as part of RFC-BPR-0003's. Both, plus the workbench presentation
documentation for RFC-BPR-0005, require updating together when this RFC is
implemented, because the join is precisely the place their two models meet.
