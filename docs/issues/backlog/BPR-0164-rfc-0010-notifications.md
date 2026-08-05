---
id: BPR-0164
title: Notifications: metadata-only content and correct deep links
status: Backlog
rfc: RFC-BPR-0010
depends_on: [BPR-0158]
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

## What this card cannot do here

Deliver one. Whether the operating system shows the banner, and whether the
link opens from it, needs Notification Center permission and a person to look.
That is BPR-0168.
