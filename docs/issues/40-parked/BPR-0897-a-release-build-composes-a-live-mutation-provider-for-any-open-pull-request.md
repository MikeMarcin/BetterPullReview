---
schema: "implement-rfc.card.v1"
id: "BPR-0897"
title: "A non-attended release build composes a live mutation provider for whatever pull request is open"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
parked_on: "human-review"
metadata: {"found_by":"dsp-006 worker on BPR-0879, while establishing whether any real pull request was ever mutated across that gap","found_at":"5c8ee790","run":"run-20260916-stations","dispatch":"dsp-006"}
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
