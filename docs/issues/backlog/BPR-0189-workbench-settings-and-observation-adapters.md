---
id: BPR-0189
title: "Adapters: workbench settings and observation"
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0188]
---

# Adapters: workbench settings and observation

`workbench_application_host` takes a `workbench_settings_port&` and a
`workbench_observation_port&`. Neither has an implementation anywhere in
`src/` — the only files naming them are the application layer's own header and
translation unit.

This is the clearest instance of the pattern BPR-0188 names: a port satisfied
by a fake never asks for an adapter. Both ports have been fully exercised by
tests since RFC-BPR-0005 landed, and neither has ever had a real
implementation, because nothing ever needed one.

## Settings

Durable drawer layout: which drawers are open and how wide, clamped to the
host's configured bound.

A separate store from the epoch catalog, the review ledger, and the queue
enrollment database, for the reason those are already separate from each other:
deleting one kind of local state must not destroy another. Losing a drawer
width should not lose reading progress.

`restore()` reads the layout and, per the host's own contract, **never writes
it back**. An adapter that persists on restore makes every installation look
deliberately configured, and there is then no way to tell a fresh one apart.

## Observation

Reports the pull request authority the workbench is showing. An observation is
not a setting and is never persisted — the same rule the appearance host states
for its own observations.

## Required direct evidence

- A store with no record is distinguishable from one holding the defaults, with
  both cases constructed. Asserting only the first passes against an adapter
  that answers everything with defaults.
- A drawer width outside the configured bound is refused or clamped exactly as
  the host specifies, and the test reads the host's published bound rather than
  restating it.
- `restore()` writes nothing, asserted by inspecting the store afterwards
  rather than by intending not to write.
- Layout survives a restart, proven across two host lifetimes against one
  store.
- A record stamped with a newer schema version fails closed, and the test
  asserts the *reason*. Removing the guard must not still fail for an unrelated
  reason — this trap has been hit twice in this repository.
- Cancellation through the supplied stop token is honoured.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
