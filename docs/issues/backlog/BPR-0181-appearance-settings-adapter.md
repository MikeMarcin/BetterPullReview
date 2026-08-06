---
id: BPR-0181
title: "Adapter: durable appearance settings behind appearance_settings_port"
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0180]
---

# Adapter: durable appearance settings behind `appearance_settings_port`

`application::appearance_settings_port` has no implementation anywhere in
`src/`. `appearance_application_host` cannot be composed without one, so this
lands first.

## The distinction the port already draws, which the adapter must keep

`load` returns `std::optional<appearance_settings>`. An absent optional means
*no record was stored*; an engaged one holds what was stored. The port's own
comment is explicit that this is what lets a fresh installation be told apart
from one that deliberately stored the defaults.

An adapter that answers a missing record with the defaults destroys that
distinction permanently, and does it silently — every caller still sees valid
settings. Do not do it. Absent is a value.

## Storage

A separate file from the epoch catalog, the review ledger, and the queue
enrollment store, for the reason those three are already separate from each
other: deleting one kind of local state must not destroy another. A reviewer
resetting their theme should not lose their reading progress.

Follow the established SQLite conventions — `PRAGMA application_id`,
`PRAGMA user_version`, WAL, `STRICT` tables, forward-only migration.

## Required direct evidence

- A store with no record answers `load` with a disengaged optional, and the
  test distinguishes that from a store holding the default settings. Both cases
  are constructed; asserting only the first would pass against an adapter that
  returns defaults for everything.
- Settings round-trip: every field stored is the field loaded, compared whole
  rather than field-by-field against a list somebody remembered to update.
- A record the core's own validator rejects is refused whole and is never
  written back.
- A database stamped with a newer `user_version` fails closed, and the test
  asserts the *reason* it failed, not merely that it failed. Removing the guard
  must not still fail for an unrelated reason — this trap has already been hit
  twice in this repository.
- Cancellation through the supplied `std::stop_token` is honoured on both
  operations.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
