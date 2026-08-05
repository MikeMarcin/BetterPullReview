---
id: BPR-0173
title: "Core: session identity, revision binding, and staleness"
status: Backlog
rfc: RFC-BPR-0011
depends_on: [BPR-0171]
---

# Core: session identity, revision binding, and staleness

A session records the exact host, repository, pull request, base OID, head OID,
and epoch pair it was started against.

## The rule this card must not erode

**Head movement makes a session stale; it never silently retargets it.** An
agent asked about one revision and answering about another would be answering a
question nobody put, and the reviewer would have no way to tell.

## Required direct evidence

- A session pinned to one head becomes stale when the head moves, and retains
  the identity it was started with rather than adopting the new one.
- A stale session refuses to be treated as current; continuing against the old
  revision is a distinct explicit choice.
- Two sessions differing only in epoch pair are different sessions.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
