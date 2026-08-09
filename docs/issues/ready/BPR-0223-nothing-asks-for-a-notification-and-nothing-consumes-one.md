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
