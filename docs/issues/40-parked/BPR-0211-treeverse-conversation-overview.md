---
id: BPR-0211
title: A secondary overview for conversations too large to follow inline
status: Parked
rfc: RFC-BPR-0005
depends_on: [BPR-0191]
parked_on: awaiting-product-decision
---

# A secondary overview for conversations too large to follow inline

> **Scope widened, 2026-08-08.** The owner narrowed the drawer to the *current
> file's* line-anchored threads, SmartBear Collaborator style, and called a
> whole-review conversation view "might be nice" rather than what they typically
> want. So this card now owns two related secondary views and not one: the
> whole-review conversation, and a Treeverse-style overview of a thread too deep
> to follow. Both are secondary to the anchored drawer, and both stay deferred.

Deferred deliberately, on the product owner's instruction, 2026-08-08.

The conversations drawer will draw its threads in a git-graph style with
filtering by lane ([[BPR-0191]]). That is expected to be enough for the
conversations a review normally carries.

The owner named the case it might not cover: a conversation unwieldy enough that
following a reply chain inline stops working. Their reference is Treeverse, which
they reach for on Twitter threads precisely because the linear presentation
becomes confusing at depth — *"great for linear conversation but confusing enough
that I had often reached for it"*.

## Why this is deferred rather than declined

It is a second view of the same data, and a second view is only worth building
once the first one has been used and found wanting. Building both now would mean
guessing which one reviewers actually reach for, and the guess would be made by
the person who wrote them rather than by anyone reviewing code.

The trigger is evidence, not size: a reviewer saying they lost their place, or a
recorded conversation whose depth makes the lane view unreadable at a supported
window width.

## What would need to be true first

- The lane view exists and has been used against a real recorded conversation —
  [[BPR-0187]] records `microsoft/DirectXMath#336`, which has two commits and no
  conversation at all, so it is not the fixture that answers this.
- A conversation deep or wide enough to be a problem is recorded, so the
  question is asked against real content rather than synthesised depth.

## Scope note, so this is not misread later

This is a *presentation* card. Nothing here changes what a conversation is, how
it is fetched, or what RFC-BPR-0002 owns. The data is already a forest of trees
with a single optional parent per node; both views read the same projection.
