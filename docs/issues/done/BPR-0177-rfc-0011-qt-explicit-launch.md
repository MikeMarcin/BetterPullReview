---
id: BPR-0177
title: "Qt: explicit launch, disclosure, and cancellation"
status: Done
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

## What this slice delivered

`qt::agent_launch_panel`, and the `start_agent_intent` /
`cancel_agent_intent` vocabulary.

### The widget cannot start anything

It holds **no process and no launcher**, and includes no header that would let
it. The only thing leaving is an intent. That is what makes "nothing launches
an agent except a user asking" a property of the type rather than a discipline
its callers keep: a refresh, a navigation, or new activity arriving cannot
reach a process from here, because there is no process here to reach.

The process owner lives in `src/adapters/agent`, and the architecture scan
enforces that the Qt adapter has no `QProcess` at all — so this is checked
rather than asserted.

### Disclosure gates the action, not the other way round

`Ask the agent` is disabled until a scope has been disclosed. A reviewer who
cannot see which tool is about to read their repository has not been asked, so
the action is unavailable until they can. Removing the disclosure condition
fails `a reviewer who cannot see which tool is about to read their repository
has not been asked`.

### The working copy is a per-run choice

The `Use my working copy` toggle is unchecked by default and is carried **on
the intent**, not read from a setting — which is what BPR-0176 refuses to
proceed without. Hard-coding it true fails `the intent carries the choice as
not made`.

### Cancelling is never the same gesture as starting

Two distinct actions. Cancellation is enabled only while a run is in flight,
and starting is disabled then, so a second invocation cannot be triggered by
someone trying to stop the first.

## Non-vacuity

| Break | Failure |
|---|---|
| Allow launch before disclosure | `a reviewer who cannot see which tool is about to read their repository has not been asked` |
| Always offer the working copy | `the intent carries the choice as not made` |

## What this card does NOT deliver

- **Nothing composes this panel.** It is built and tested and no window shows
  it; RFC-BPR-0011's closeout or a later composition card owns that. Saying
  otherwise would be the composition gap BPR-0180 found six RFCs of.
- **No streaming, no findings, no chat.** Watching an agent think is
  RFC-BPR-0012's.
- **No approval flow.** The agent's own approvals are its business; this
  product does not proxy them.

## Required direct evidence

- The adapter holds no process and no launcher; it emits an intent and nothing
  more, so there is no path by which a widget could start anything.
- The provider and the scope are shown before launch, not after.
- Cancellation is offered while a session runs and is a distinct action.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
