---
schema: "implement-rfc.card.v1"
id: "BPR-0136"
title: "Core target and head revalidation and the stale-head refusal"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: ["BPR-0135"]
---

# Core target and head revalidation and the stale-head refusal

See the RFC-BPR-0008 verification plan for this slice's place in the sequence.

Every surface here is built against the fixture provider. No slice in this RFC
performs a live mutation; the criteria that need one are BPR-0142, BPR-0143,
and BPR-0144.

## Required direct evidence

- The behaviour this slice owns is exercised directly, not inferred from a
  neighbouring slice passing.
- Every refusal carries its own typed reason, so two different mistakes are
  distinguishable by a caller.
- No test reaches a live host.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## Outcome

`revalidate` compares the target a publication was prepared against to the one
observed at publish time, and `publication_permitted` decides what an
acknowledgement can and cannot authorise.

### Two mismatches, not equally recoverable

A **moved head** means the right pull request at a newer revision. The reviewer
can be shown the new revision and say to go ahead, so an explicit
acknowledgement permits it.

A **changed target** — a different host, repository, or pull request — can
**never** be acknowledged away, whatever the reviewer clicked. The
acknowledgement they gave was about a different pull request, so treating it as
consent for this one would be inventing agreement they never expressed. The
test asserts refusal both with and without acknowledgement, because a check
that only tested the unacknowledged path would miss exactly the bug worth
fearing.

An **unnameable head** is refused outright for the same reason: a revision
nobody can name is not one a reviewer can have agreed to.

### Check order is policy

The target is compared before the head. A comment aimed at the wrong pull
request is a worse mistake than one aimed at the right pull request's newer
revision, and reporting the head first would hide it. A test sets both wrong at
once and asserts the changed target is what comes back.

### Non-vacuity

Three behaviours were broken separately, each failing its own assertion, then
restored to green:

- An acknowledgement authorising a changed target — "no acknowledgement
  authorises publishing to a different target".
- The head checked before the target — "both wrong reports the changed target,
  not the moved head".
- A moved head publishing unconditionally — "a moved head does not publish on
  its own".

The third break initially failed to compile, leaving `aAcknowledged` unused
under `-Werror`, and the suite reported the previous break's message from a
stale binary. Redone so it compiles. This is the second time in this RFC that
a break has been silently swallowed that way; the build exit code is checked
before any result is believed, which is what caught both.

### A test fixture bug worth recording

The first run failed on "an identical target revalidates" — a real failure with
a wrong cause. The helper built a sha1 oid by filling all thirty-two bytes, but
a sha1 oid occupies the first twenty and requires the rest to be zero, so it
produced an *invalid* oid and the comparison reported `head_unknown`. The
policy was right and the fixture was wrong. Recorded because a fixture that
silently produces invalid identities would have made every later assertion in
this file meaningless.
