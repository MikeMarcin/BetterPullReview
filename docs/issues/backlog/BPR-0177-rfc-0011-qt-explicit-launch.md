---
id: BPR-0177
title: "Qt: explicit launch, disclosure, and cancellation"
status: Backlog
rfc: RFC-BPR-0011
depends_on: [BPR-0171]
---

# Qt: explicit launch, disclosure, and cancellation

Every invocation is a visible user command. The adapter shows which provider
and which scope, and offers cancellation.

## The rule this card must not erode

**Nothing launches an agent except a user asking.** Not a refresh, not
navigation, not new activity arriving. An agent that started itself would spend
the user's money and their agent's rate limit on work they did not request.

## Required direct evidence

- The adapter holds no process and no launcher; it emits an intent and nothing
  more, so there is no path by which a widget could start anything.
- The provider and the scope are shown before launch, not after.
- Cancellation is offered while a session runs and is a distinct action.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
