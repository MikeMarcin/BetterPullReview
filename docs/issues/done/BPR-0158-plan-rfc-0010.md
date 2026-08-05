---
id: BPR-0158
title: Plan RFC-BPR-0010 verification and allocate its cards
status: Done
rfc: RFC-BPR-0010
depends_on: []
---

# Plan RFC-BPR-0010 verification and allocate its cards

RFC-BPR-0010 gives the reviewer a landing page that answers what needs their
attention across reviews — including pull requests they watch because of an
author or a path — without asking GitHub to change reviewer assignment or
subscription.

## Outcome

`docs/verification/RFC-BPR-0010.md` records the allocation, the claim matrix,
the deferred gates, and cards BPR-0159 through BPR-0167.

## The rule the whole plan serves

**Observing is local.** An observer rule enrolls a pull request in this
reviewer's queue and changes nothing on GitHub: no reviewer assignment, no
watch, no subscription, no label, no CODEOWNERS entry. The RFC's non-goals say
so directly.

It is the criterion a convenience would erode first. "Just request review so it
shows up in the normal place" is a plausible feature request, and it would make
the tool speak on the user's behalf to their colleagues — which is precisely
what a reviewer avoiding a subscription-culture change is trying not to do.

The plan expresses it structurally rather than by check: the correlation that
decides enrollment is a pure function with no provider, so there is no path by
which matching a rule could issue a request. A test that had to *watch* for an
absent request would be testing a guard; there is nothing to guard.

## The second rule

**A notification carries no source and no comment body.** It says which review
wants attention and links to it. Anything more copies private repository
content into the operating system's notification store, which is outside every
boundary this product documents — it survives the application, is readable by
other software, and on some systems is synchronised to other devices.

Structural expression again: the notification request type has no field a body
could occupy.

## Why this RFC can be implemented now

`depends_on` names RFC-BPR-0007 and RFC-BPR-0008. Both are implemented on
available targets and stay `Proposed` behind owner-supplied gates and the
ordered closure chain. Done BPR-0088 records the product owner's direction to
advance the implementation frontier on available targets. No card here makes a
closure claim.

## What the RFC does not need that it might appear to

The refresh scheduler is fully testable on this host. The RFC says rate limits
are **simulated** rather than exercised against a live host, so primary and
secondary limit handling, backoff, offline state, and cancellation are all
fixture work. Nothing in the scheduler needs a token.

## Deferred gates

| Card | Gate | What is missing |
|---|---|---|
| BPR-0168 | A native notification is delivered and its deep link opens the right local review | A person to grant Notification Center permission once, and to see the banner. The harness can prove the request's content and destination; it cannot prove the operating system showed it |
| BPR-0169 | Enrolling by observer rule leaves GitHub's reviewer and subscription state untouched | A designated repository and token, inheriting BPR-0142. Structurally the correlator cannot issue a request; that the *server* also shows no change is a different claim and needs a server |

Both are parked as backlog cards naming exactly what is missing, per the
standing direction, rather than blocking the rest.
