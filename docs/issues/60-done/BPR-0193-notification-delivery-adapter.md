---
id: BPR-0193
title: "Adapter: deliver a queue notification to the operating system"
status: Done
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

## What this slice delivered

`application::review_queue_notifier` takes the request the decision already
produces, asks the platform once, and hands it a `delivered_notification`.
`qt::system_tray_notification_sink` shows it through the desktop's own
notification surface. Both are composed into the application.

### The guarantee, kept structurally

The notifier **holds a sink and a callback, and nothing else**. There is no
pull request record, no review ledger, and no diff reachable from it, so there
is no path to repository content even for someone trying to improve the
wording. The payload is composed by `notification_title`,
`notification_body`, and `notification_target` — the request's own functions.

The sink is narrower still: it receives `Title` and `Body` already composed and
never sees the request they came from. `Target` is retained only so an
activation can name the review, and it stays a local reference — nothing here
builds a URL.

### The order the bucket and the permission are consulted in

The bucket decides **first**, before the platform is touched at all. A quiet
queue therefore cannot raise a permission prompt, which matters because the
prompt is the thing a reviewer sees once and answers forever. Reversing those
two lines fails `and the platform is not even consulted`.

### A gate entry that could never have matched

`verify_composed_hosts.py` listed `review_queue_notification` as not-composed,
pending this card. That string is a **translation unit name**, and the
decision's contents are free functions whose demangled symbols read
`bpr::application::notification_title`. The entry would have gone on reporting
"absent" after composition, and moving it to `COMPOSED` would have failed the
gate with the symbol genuinely present.

It now names `notification_title` and `warrants_notification`, which exist.
Worth recording because the failure mode is silent in the direction that
matters: a not-composed entry that can never match is a gate that never fires.

### A comment that tripped the architecture scan

The header said "Nothing here builds a URL". `architecture_boundaries` refuses
that word anywhere in the Qt adapter's GitHub projection surface, and it failed
the build on the comment rather than on any code.

The scan is right and the comment was what had to change. This is the second
time this session a *comment* has tripped a boundary the code respects —
BPR-0118's was a core header naming a presentation type — and the lesson is the
same both times: the scan cannot tell a description from an intention, and the
place a reader learns that something is thinkable is exactly the comment.

## What was measured

| Claim | Evidence |
|---|---|
| Payload asserted whole | The delivered record is compared field for field against what the request's own functions produce, plus a guard that the comparison is not two empty strings |
| Refusal is normal | Five deliveries against a refused platform: nothing delivered, nothing reported, and `Asks == 0` |
| Asked once | From `not_determined`, one ask; the answer is then honoured without a second |
| Quiet asks nothing | The platform's `permission()` is never reached |
| Activation names the review | The target reaches the sink the composition installed; the notifier selects nothing |
| Composed and constructed | `COMPOSED_HOSTS_OK: 10 composed`, and `NOTIFICATION_DELIVERY_COMPOSED_OK` from the running application |

### Non-vacuity

| Break | Failure |
|---|---|
| Deliver regardless of permission | `a refused reviewer is not notified` |
| Compose the body from the title | `the delivered payload equals what the request's own functions produce` |
| Consult the platform before the bucket decides | `the platform is not even consulted, so a quiet queue cannot raise a permission prompt` |

## What this card does NOT deliver

- **Nothing in the product produces a request yet.** RFC-BPR-0010's queue
  composition owns that, and it is still in `backlog/`. The delivery path
  exists and is constructed; every delivery in this build would come from a
  probe. Saying otherwise would be the error this card's own closing section
  warns about, one level up from the one it names.
- **No macOS `UserNotifications` banner.** The sink uses the desktop
  notification surface Qt exposes, which reports `refused` where none exists —
  including under the offscreen platform the suite runs on. The real banner on
  a real display is BPR-0168's attended check, deliberately not simulated.
- **Activation is recorded, not acted on.** The composition stores the chosen
  target; opening that review needs the queue surface.

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
