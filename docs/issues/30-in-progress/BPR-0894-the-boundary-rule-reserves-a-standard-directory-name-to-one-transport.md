---
schema: "implement-rfc.card.v1"
id: "BPR-0894"
title: "The architecture boundary rule reserves the Linux runtime-directory variable name to one transport, so nothing else may discover it"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-006, hit while implementing BPR-0881","found_at":"68f8152e","run":"run-20260916-stations","dispatch":"dsp-006"}
---

# BPR-0894: The architecture boundary rule reserves the Linux runtime-directory variable name to one transport, so nothing else may discover it

## Defect

`cmake/VerifyArchitectureBoundaries.cmake:5065` reserves the *name* of the
standard Linux per-user runtime directory environment variable to
`src/adapters/github/src/gh_cli_transport.cpp`. `architecture_boundaries` fails
any other file that spells it.

The rule's intent is sound and worth keeping: the GitHub CLI transport
constructs a child process environment, and which variables it passes is a
boundary decision that belongs to one file. But the rule is written against the
*token*, not against that behaviour, so it also forbids any component from
**reading** the variable to discover a directory the platform documents for
exactly this purpose.

[[BPR-0881]] hit this directly. The profile activation rendezvous needs a short
per-user runtime directory; on Linux the documented way to find one is that
variable. The adapter cannot read it, so it probes the standard location
(`/run/user/<uid>`) and verifies it owner-private like any other candidate —
which works, and is arguably more defensive since it verifies rather than
trusts, but it is a workaround for a rule that meant to say something narrower.
A system that puts the runtime directory elsewhere falls through to `$TMPDIR`
instead of being found.

## Scope

- Narrow the rule to what it means: the GitHub CLI transport owns *constructing*
  the child process environment, including deciding whether that variable is
  passed through. Reading it to locate a directory is not that.
- Check the other names the same rule reserves for the same over-reach before
  changing only this one; a single-token fix that leaves the same shape behind
  is not worth a card.
- Keep `architecture_boundaries` green, and keep a case that would fail if the
  transport's environment construction leaked into another file — that is the
  property worth defending.
- If the rule cannot be narrowed without weakening it, say so and leave the
  probe in place with this card as the record of why it is written that way.

## Acceptance

- The rule expresses the boundary it intends, or the card records why it cannot
  and BPR-0881's direct probe stands as deliberate.
- `architecture_boundaries` still fails a real leak of the transport's
  environment construction.
- No component is blocked from discovering a platform directory by the standard
  means, or the card names why that is intended.
