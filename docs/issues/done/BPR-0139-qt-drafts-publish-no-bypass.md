---
id: BPR-0139
title: Qt draft editors, publish confirmation, and the no-bypass proof
status: Done
rfc: RFC-BPR-0008
depends_on: [BPR-0138]
---

# Qt draft editors, publish confirmation, and the no-bypass proof

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

`review_draft_editor` composes a review comment and asks for it to be
published. It is given two dispatch callbacks and nothing else.

### No bypass, by construction

There is no provider to hold and no endpoint to reach. The editor's only
outbound path is a semantic command, which makes "a widget never calls a
mutation endpoint" true because the type has no way to, not because a reviewer
checked. The test intercepts the dispatch because there is nothing else to
intercept.

### An action emits an intent and changes nothing

Triggering publish, discard, mark-read, accept, or exclude emits the
corresponding command and leaves the editor exactly as it was. The displayed
state moves only when `present` is called with an accepted projection.

A widget that updated itself optimistically would show a comment as published
while the request was still in flight — and would keep showing it if the
request failed. Breaking this makes the editor advance its own state and the
suite reports "nothing the editor shows has moved".

### Enablement follows the projection, not the keystrokes

Typing does not enable publishing. A publish button enabled because the user
typed would be the widget deciding legality the core owns; the enablement is
derived from the presented draft's state. A published comment is GitHub's, so
the editor stops offering to change it and the body becomes read-only.

An action with nothing presented emits nothing at all, so a stray trigger
cannot manufacture a command about a draft that does not exist.

### The target is shown before publishing

The reviewer is agreeing to a specific pull request at a specific revision
under a specific account, and cannot agree to what they were not shown. The
label carries all three and is announced to assistive technology rather than
only drawn.

### Naming

A `QAction` is not a widget and carries no accessible name of its own —
assistive technology reads its text — so the text *is* the identity a person
hears. All five are asserted pairwise distinct in both text and object name.
"Accept file" and "Exclude file" are deliberately not "Accept" and "Exclude",
which would be ambiguous beside a file list.

### Non-vacuity

Three behaviours were broken separately, each failing its own assertion, then
restored to green:

- The editor advancing its own state on publish — "nothing the editor shows has
  moved".
- Enablement derived from the typed text — "the accepted projection is what
  changes the editor".
- Two actions sharing one text — "two actions never share the text assistive
  technology reads".

### This closes BPR-0127's deferred half

BPR-0127 recorded that "marking read, accepted, or excluded emits an intent and
changes nothing until the accepted projection arrives" was proven at the host
but had no widget. It has one now, and the three receipt actions are asserted
here under the same rule as the draft actions.
