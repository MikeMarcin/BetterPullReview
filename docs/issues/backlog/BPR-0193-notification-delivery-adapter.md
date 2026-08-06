---
id: BPR-0193
title: "Adapter: deliver a queue notification to the operating system"
status: Backlog
rfc: RFC-BPR-0010
depends_on: [BPR-0164]
---

# Adapter: deliver a queue notification to the operating system

`application::review_queue_notification` decides whether a queue item warrants
a notification and what the request carries. Nothing consumes it, so no
notification is ever delivered. Zero notification symbols reach the shipped
binary and `src/` names no native notification API at all.

## What this slice does

Take the request the application already produces and hand it to the platform,
then route an activation back as an intent that selects the named review.

## The rule the request type already holds, which the adapter must not undo

BPR-0164's guarantee is structural: `queue_notification_request` has **no field
a comment body, a source line, a title, or a path could occupy**. The operating
system's notification store outlives the application, is readable by other
software, and on some systems synchronises to other devices — it is outside
every boundary this product documents.

The adapter is where that guarantee could be quietly lost, because it is the
first place with both the request and a free-form platform API. It must compose
its payload from the request's fields alone. Do not read the pull request, the
ledger, or the diff to "improve" the text.

## The link is a local reference, not a URL

BPR-0164 decided this and the decision stands. A `https://` address would open
a browser, which is the one thing a native review tool exists to avoid, and a
link the operating system can hand to any registered handler is one another
application can claim.

## Permission

macOS asks once and never again. The adapter must behave correctly in all three
states — not yet asked, granted, refused — and a refusal is a normal outcome
rather than an error to retry. A reviewer who declined notifications has made a
choice, and a tool that re-asks or degrades has overridden it.

## Required direct evidence

- `nm` on the shipped binary reports the delivery adapter's symbols, and the
  notification decision's symbols with them.
- A delivered notification's payload is asserted **whole** against exactly what
  the request carried, rather than searched for the absence of a bad string. A
  test that only checks one string is absent passes against a payload carrying
  a different one.
- No path from the adapter reaches the pull request record, the review ledger,
  or diff content. Proven structurally — the adapter should not hold anything
  that could supply them.
- Activation produces an intent naming the review, and the adapter itself
  selects nothing.
- A refused permission delivers nothing, reports no error, and does not re-ask.
- Delivery runs against a fake platform sink in the suite. The real banner is
  BPR-0168's attended check and is not simulated here.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## What must not be done instead

Calling this done because a request was constructed. That is BPR-0164, it is
already verified, and mistaking it for delivery is exactly the error that left
BPR-0168 misfiled as owner-attended for as long as it was.
