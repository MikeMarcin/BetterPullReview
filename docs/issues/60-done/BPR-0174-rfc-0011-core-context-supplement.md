---
id: BPR-0174
title: "Core: the context supplement and what it may carry"
status: Done
rfc: RFC-BPR-0011
depends_on: [BPR-0171]
---

# Core: the context supplement and what it may carry

BetterPullReview supplies only what GitHub cannot: selected epochs, the
last-reviewed head, changed-since-review files, local defect state, the current
selection, and explicitly included private notes.

## The rule this card must not erode

**Private notes and drafts are never included without explicit selection.** A
supplement that swept them in by default would send a reviewer's unfinished
thinking to a tool they may only have wanted to ask about a diff.

## What this slice delivered

`core::agent_supplement_request`, `core::agent_context_supplement`, and
`core::build_agent_supplement`. The supplement carries the binding, the
last-reviewed head, changed-since-review files, local defect state, the
current selection, a schema version — and notes, only where the request named
them.

### Selection is structural, not filtered

`Notes` is copied from `SelectedNotes` and from nowhere else. The builder
holds no note store, so a request that selected none produces a supplement
with none **because there was no source**, rather than because a filter
removed them.

A redaction step would be worse in the way that matters: it is a place a
mistake can live, and the mistake is silent. The supplement looks right to
whoever wrote the code and wrong only to the reviewer whose unfinished
thinking is now in a tool they meant to ask about a diff.

### The test reads the whole supplement

The default case compares against a fully-specified empty supplement, field
for field, rather than checking `Notes.empty()`. A one-field check would pass
against a supplement that had put the draft somewhere else. Sweeping in an
unselected note fails `there is nowhere else a note could be`.

### A field guard that is not `sizeof` arithmetic

My first credential guard summed member sizes and asserted against `sizeof`.
It failed immediately, on padding I had guessed wrong — and it would have been
fragile in the direction that matters: failing for the wrong reason on a
layout change, and passing for a field swapped with one of equal size.

It is a structured binding now. Binding every field by name over an aggregate
requires the exact count, so adding a field stops the test compiling and the
next person has to come here and say why theirs cannot hold a credential or an
unselected note.

### Refused whole, never shortened

A silently truncated list of changed files would tell an agent that a file it
was not shown is unchanged. Over a bound, the whole supplement is refused.

## Non-vacuity

| Break | Failure |
|---|---|
| Sweep in a note nobody selected | `a supplement built without explicit selection equals the empty one, field for field` |
| Stop refusing an oversize request | `too many notes is refused rather than shortened` |

## What this card does NOT deliver

- **No serialisation.** What the supplement looks like on the wire is the
  host's, and BPR-0175 owns getting it there over stdin rather than argv.
- **No note store.** Where private notes live and how a reviewer selects one is
  RFC-BPR-0007's and the Qt adapter's.
- **No defect text.** `supplement_defect` carries state and location; what was
  said about a defect is review content the agent reads from GitHub itself.

## Required direct evidence

- A supplement built without explicit selection contains no private note and no
  draft, and the test reads the whole supplement rather than checking for one
  absent field.
- Every supplement carries host, repository, pull request, base OID, head OID,
  epoch pair, and a schema version.
- No field of the supplement can hold a credential.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
