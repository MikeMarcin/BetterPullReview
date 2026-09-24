---
schema: "implement-rfc.card.v1"
id: "BPR-0897"
title: "A non-attended release build composes a live mutation provider for whatever pull request is open"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-006 worker on BPR-0879, while establishing whether any real pull request was ever mutated across that gap","found_at":"5c8ee790","run":"run-20260916-stations","dispatch":"dsp-006","owner_decision":{"date":"2026-09-24","session":"cbd48b91000b00f1e72e1ad62c7c384c","candidate":"BPR-0897-observed-mutation-intent-v001","decision":"e1d2139c-92e2-40c8-8f84-6c59c2736fcb","selected":"1. Yes, as designed"}}
---

# BPR-0897: A non-attended release build composes a live mutation provider for whatever pull request is open

## The question for the owner

This card is **parked for a product decision, not a defect report.** Nothing
here is known to be wrong; what is missing is a recorded intent.

[[BPR-0879]] asked whether any real pull request could ever have been mutated
across its unbound-thread gap. The answer is no for every lane this project
runs — but establishing that turned up the boundary the answer rests on, and it
is worth stating explicitly before the product ships.

There are two composition paths to a live review mutation:

- **The attended designated-live lane**, gated twice. `designated_live_mutation_gate`
  proves the pull request's identity field by field before launch, and inside
  the application `select_designated_review_mutation` refuses unless hostname,
  owner, name, repository id, pull-request id, number, both node ids, both OIDs
  and both refs equal compile-time constants. On any mismatch there is no
  selection, so no provider and no mutation host exist at all. This is the lane
  every piece of live evidence in this tree was produced through.
- **`select_observed_review_mutation`**, which in a non-attended
  release-configuration build composes the live provider for **whatever pull
  request is open**, with the resolve gesture minting its own one-shot grant.

The second is presumably the product: a review tool that cannot publish to the
pull request you are looking at is not a review tool. But it is also the reason
[[BPR-0879]]'s answer carries a residual — it rules out writes by this
project's lanes, and cannot rule out someone running a release-configuration
build against their own account. No such build is tagged or recorded here.

Related and already parked: [[BPR-0641]], enabling normal inline publication
for private alpha, is the same boundary seen from the other side.

## The exact ask

**Is `select_observed_review_mutation` composing a live provider for any open
pull request the intended behaviour before this product reaches anyone outside
this repository?** Three answers are all reasonable and they lead to different
work:

1. **Yes, as designed** — then this card closes by recording that intent next
   to the composition, so the next person who traces this boundary finds the
   decision rather than re-deriving it.
2. **Yes, but not yet** — then it wants the same kind of gate the attended lane
   has, active until [[BPR-0641]] opens it, and this card becomes that work.
3. **Only for repositories the person has explicitly designated** — then it
   wants a designation step in the product, which is a feature, not a guard.

## Acceptance

- The owner's answer is recorded here, and the intent is written next to the
  composition rather than living only on this card.
- If the answer is (2) or (3), the resulting guard or designation step has its
  own card and its own contract row.
- [[BPR-0879]]'s residual is updated to point at the answer.

## One fact found after this card was written, dsp-007

It does not answer the ask and does not narrow the three options, but the owner
should have it, because it bounds the residual this card describes.

[[BPR-0896]] has been verified: a **reply** through the live session cannot be
submitted at all against a real server. Its pre-admission baseline
reconciliation compiles `GET /graphql?per_page=100` with an empty body and
fails before the POST is ever reached
(`src/adapters/github/src/github_mutation_contract.cpp:2116`, before admission
at `:2124`). So for the single operation a reviewer uses most, the residual
this card worries about — someone running a release-configuration build against
their own account — could not have produced a write. It would have produced an
error.

This says nothing about resolve, unresolve, or root comments, which take
different paths and are not known to be broken. So it shrinks the exposure; it
does not remove it, and it is not a reason to answer (1) by default. If
anything it argues the opposite way about urgency: the reply path has to be
rebuilt for [[BPR-0896]] regardless, and it is cheaper to build whatever guard
answer (2) or (3) implies while that code is already open than to add it
afterwards.

## Owner decision, 2026-09-24: yes, as designed (attended session cbd48b91)

Source: attended session `cbd48b91000b00f1e72e1ad62c7c384c`, packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
hand-back at sequence 589. Candidate `BPR-0897-observed-mutation-intent-v001`,
revision `5c8ee790` (dsp-006; fact added in dsp-007), fingerprint `f222c2b9…`,
decision `e1d2139c-92e2-40c8-8f84-6c59c2736fcb`, sequence 575,
2026-09-24T18:59:01Z, `actor: human`: **select**, verbatim:

> 1. Yes, as designed: record the intent next to the composition

The note is empty. **The answer is recorded.** `select_observed_review_mutation`
composing the live mutation provider for whatever pull request is open, in a
non-attended release-configuration build, is the intended product behaviour.
Answers 2 (a gate until [[BPR-0641]] opens it) and 3 (a designation step) are
not chosen, so no guard or designation card follows from this card.

Acceptance, as it now stands:

- *The owner's answer is recorded here*: done (this section).
- *The intent is written next to the composition*: **open**. This is the one
  piece of work left: a short comment at `select_observed_review_mutation` in
  the composition (`src/app`). The comment says that composing the live
  provider for the open pull request is intended, cites this card and the
  date, and contrasts it with the designated lane's two gates. If the
  composition file is pinned by a round-trip contract, reseal it in the same
  commit. The card leaves human-review for **Ready**.
- *If (2) or (3), the guard has its own card*: not applicable.
- *[[BPR-0879]]'s residual points at the answer*: done. BPR-0879's "Residual
  honesty" paragraph now carries a dated pointer here.

The dsp-007 fact still stands: a reply cannot be submitted through the live
session until [[BPR-0896]]'s reconciliation is rebuilt. That limits what the
answer exposes today; it does not change the answer.
