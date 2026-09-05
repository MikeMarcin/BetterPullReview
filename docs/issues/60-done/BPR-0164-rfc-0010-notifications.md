---
schema: "implement-rfc.card.v1"
id: "BPR-0164"
title: "Notifications: metadata-only content and correct deep links"
status: "Done"
rfcs: ["RFC-BPR-0010"]
depends_on: ["BPR-0158"]
---

# Notifications: metadata-only content and correct deep links

A notification says which review wants attention and links to it.

## The rule this card exists to hold

**No source, no comment body.** The operating system's notification store
outlives the application, is readable by other software, and on some systems is
synchronised to other devices. It is outside every boundary this product
documents, and a diff line or a comment quote placed there has left the
product's control permanently.

The request type has no field a body could occupy. That is the guarantee — not
a redaction step, which would be a place for a mistake to live.

## Required direct evidence

- The notification request type has no body field, and no composition path
  supplies one.
- A notification requested for a review with comments carries counts and
  identity only, and the test reads the whole request rather than checking for
  the absence of one string.
- The deep link resolves to the host, repository, and pull request the queue
  item names, and following it selects that review locally.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## What was done

`application::queue_notification_request` names the review, says which bucket
it is in, and counts what is waiting. There is no field a comment body, a
source line, a title, or a path could occupy.

### Why the absence rather than a redaction

Carrying the text and stripping it on the way out is the obvious alternative
and is worse in the way that matters. A redaction step is a place a mistake can
live, and the mistake is silent: the notification looks right to whoever wrote
the code and wrong only to the person whose repository content is now in a
system database that outlives the application, is readable by other software,
and on some systems syncs to their other devices. Nothing retrieves it
afterwards.

### The title is identity only

Repository and number. The pull request's own title is the obvious thing to add
and is exactly what must not be there — titles routinely name the customer, the
incident, or the unreleased feature.

### Not every bucket notifies

`quiet` asks nothing of anyone. `authored, awaiting reviewer action` asks
nothing of *this* reviewer — there is nothing for them to do, and a
notification would be inviting them to look at their own patience. A tool that
interrupted for either would teach the reviewer to ignore its notifications,
after which the ones that matter are ignored too.

### The link is a local reference

Not a URL. A `https://` address would open a browser, which is the one thing a
native review tool exists to avoid, and a link the operating system can hand to
any registered handler is one another application can claim.

### Evidence

Six tests. The no-content test reads the *whole* notification and compares it
to exactly what it should be — identity, the bucket's fixed sentence, a count —
rather than searching it for a list of bad words. A test that only checked for
the absence of one string would pass against a notification carrying a
different one.

Five breaks, each landing on its own assertion:

| Break | First failure |
|---|---|
| quiet reviews notify too | `a quiet review asks nothing and interrupts nobody` |
| the count is dropped | `and the notification is exactly its identity, its bucket's sentence, and a count` |
| two buckets share a sentence | same |
| the link loses the account | `the link resolves to the review it names` |
| the observer rule's path is shown | `nothing a pull request contains reaches the notification` |

The last one is the one worth having. To make it, the request type had to be
given a field a path could occupy and the construction had to be changed to
fill it — which is the point: leaking content here is not a slip, it is three
deliberate edits. And it is a plausible mistake, because the landing model
legitimately shows the matched rule on screen, and copying that behaviour into
a notification is exactly how a path would end up in the system store. The test
item now carries a real-looking path so that copy is caught.

## What this card cannot do here

Deliver one. Whether the operating system shows the banner, and whether the
link opens from it, needs Notification Center permission and a person to look.
That is BPR-0168.
