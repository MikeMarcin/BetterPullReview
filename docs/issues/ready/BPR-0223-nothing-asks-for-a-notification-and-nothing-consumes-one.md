---
id: BPR-0223
title: Nothing asks for a notification and nothing consumes an activation
status: Ready
rfc: RFC-BPR-0010
depends_on: []
---

# Nothing asks for a notification and nothing consumes an activation

Found while preparing [[BPR-0168]] for an attended session, and the reason that
session was pulled back before the owner was asked.

The notification path is composed at both ends and connected at neither.

## Measured

Three checks say the delivery path is present, and all three pass:

```
nm | c++filt | grep notification            120 symbols
verify_composed_hosts.py                    COMPOSED_HOSTS_OK
runtime marker                              NOTIFICATION_DELIVERY_COMPOSED_OK
```

They are all true and none of them is the claim.

```
$ grep -rn "deliver(" src/app/ | grep -v notification_delivery.hpp
(nothing)
```

`application::review_queue_notifier::deliver()`
(`notification_delivery.hpp:100`) is the outbound entry point, and nothing in
`src/app/` calls it. The only call into the notifier is
`Notifications->activate(aTarget)` at `github_application.cpp:996`, which is a
banner being *clicked*, not a banner being *shown*.

At the other end, `ActivatedNotification` appears exactly twice in
`github_application.cpp`: its declaration at `:1617` and its assignment at
`:991`. Nothing reads it. Clicking a banner records a target into a member no
code consumes, so no review is selected.

## This is a real gap, not an accident

Both halves are documented in the composition root, deliberately:

> *"what still does not exist is a composed review queue to produce requests,
> which RFC-BPR-0010's own composition owns. Every delivery in this build
> therefore comes from the fixture probe."* — `:977-981`

> *"The notifier selects nothing; this is where a chosen review would be
> opened. RFC-BPR-0010's queue surface owns that, so the target is recorded and
> the separation is kept rather than short-circuited."* — `:988-992`

The separation was the right call when it was made — short-circuiting it would
have faked the claim. What is missing is the card that closes it, and this is
that card.

## Required direct evidence

- A queue item that warrants a notification causes `deliver()` to be called
  through the composed path, with no test seam involved. The runtime marker
  must distinguish "the sink exists" from "the sink was asked", because
  `NOTIFICATION_DELIVERY_COMPOSED_OK` already proves the first and proves
  nothing about the second.
- An activation selects the review it names. `ActivatedNotification` is read by
  something, and the selection is observable from outside the class.
- Both shown non-vacuous: cut the wire at each end separately and watch the
  corresponding assertion fail.
- The metadata rule from [[BPR-0164]] still holds at the composed path: the
  request carries identity and counts, no source line and no comment text.
  Composing the two ends must not widen what crosses them.

## What must not be done instead

Adding a debug flag that calls `deliver()` so a banner appears. It produces a
real banner from a path no user can reach, which is the same shape of evidence
this RFC has twice had to retract — and it leaves the activation end still
consuming nothing, so the half that matters most is untouched.

Counting `NOTIFICATION_DELIVERY_COMPOSED_OK` as evidence for this card. That
marker is why the gap survived: it is true, it is checked, and it answers a
different question.

## Correction, 2026-08-09: this card understated the work, the same way BPR-0168 did

Written yesterday as "the notification path is composed at both ends and
connected at neither", with the fix framed as wiring. That framing is wrong,
and the error is the one this card exists to document, made one level up.

There is nothing in the middle to wire to. Measured against the shipped binary:

| Symbol | In `app_better_pull_review` |
|---|---|
| `review_queue_refresh` | 0 |
| `review_queue_enrollment` | 0 |
| `review_landing_model` | 0 |

`grep -rn` across `src/app/` finds no reference to any of them. `notification_for`
takes a `core::queue_item`, and **the application constructs no queue items**,
because it composes no review queue.

The standing gate already says so, and I did not read it carefully enough
before writing this card. `scripts/verify_composed_hosts.py` lists both
`review_queue_refresh` and `review_queue_enrollment` under `NOT_COMPOSED` as
"RFC-BPR-0010 composition" — a tracked, deliberate gap, not an oversight.

So the shape is:

- **BPR-0164** built the decision. Composed.
- **BPR-0193** built the delivery adapter. Composed.
- **The queue that would produce items to decide about** is not composed, and
  neither is the landing surface an activation would select in.

Both of this card's clauses depend on that. "Something asks for a delivery"
needs queue items; "something consumes an activation" needs a surface to select
a review in. Neither is a wiring change.

### What this card actually is

RFC-BPR-0010's composition slice: compose the queue refresh host, enrollment,
and the landing model into the application, then connect the notification
decision to the composed queue and the activation to the composed landing
surface. Landing it moves three names from `NOT_COMPOSED` to `COMPOSED` in
`verify_composed_hosts.py`, which is the bookkeeping that makes this checkable.

That is a feature slice, not the repair this card was filed as. It is left in
`ready/` with its scope corrected rather than started, because starting it and
stopping halfway would leave the product with a queue that half exists — and a
half-composed queue is exactly the state that produced BPR-0180, BPR-0188, and
this card's own subject.

### Why the same mistake twice is worth recording

BPR-0168 was misfiled as attended when the delivery adapter did not exist, and
misfiled again as ready when the delivery adapter existed but nothing called
it. This card then diagnosed that second miss and repeated the pattern: it
checked both **ends** of a path and did not check whether the path had a
middle.

The instrument that would have caught it was one command, and it is the same
one that caught the previous two: ask the linker what is actually in the
binary, and ask it about the thing you are depending on rather than the thing
you are looking at.
