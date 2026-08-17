---
id: BPR-0173
title: "Core: session identity, revision binding, and staleness"
status: Done
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

## What this slice delivered

`core::agent_revision_binding`, `core::agent_session`, and
`core::observe_agent_session`. A binding names the host, repository, pull
request, base OID, head OID, and epoch pair; `operator<=>` compares all of
them, so two bindings differing anywhere are different bindings.

### Observing returns a verdict, and changes nothing

`observe_agent_session` compares and reports. There is no path in this header
that rewrites a session's binding, which is what makes "never silently
retargets" structural rather than a rule someone has to remember. A caller who
wants an answer about the new revision starts a new session, and that is
visible as a new session.

`continue_against_binding` returns the session's own binding
**unconditionally**. It is a named function rather than a field access so that
continuing against an old revision is a call a reader can find, instead of
looking like reading current state. Making it retarget fails `continuing
answers about the revision the agent was asked about`.

### The epoch pair is identity, not detail

An agent asked "what changed since the last review" and one asked "what
changed in the whole pull request" were asked different questions even where
the head is identical. So the pair is part of the binding, and the test
asserts both bindings agree on every revision before asserting they differ —
otherwise it would be proving something about the OIDs.

### A different review is not staleness

The review check runs before the head check. A session bound to another pull
request reports `different_review` rather than `head_moved`, because reporting
movement would invite a caller to offer the reviewer a refresh that could not
mean anything.

## Non-vacuity

| Break | Failure |
|---|---|
| A moved head stops making a session stale | `a session pinned to one head is stale when the head moves` |
| Continuing retargets to a different head | `continuing answers about the revision the agent was asked about` |

Each break compiled; the error count was checked before the failure was
believed.

## What this card does NOT deliver

- **No session store.** Where sessions live, and how many, is the host's
  question.
- **No result events.** A session is what was asked; what came back is
  RFC-BPR-0012's.
- **No refresh gesture.** Starting a new session against the moved head is a
  thing a surface offers, and no surface offers it yet.

## Required direct evidence

- A session pinned to one head becomes stale when the head moves, and retains
  the identity it was started with rather than adopting the new one.
- A stale session refuses to be treated as current; continuing against the old
  revision is a distinct explicit choice.
- Two sessions differing only in epoch pair are different sessions.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
