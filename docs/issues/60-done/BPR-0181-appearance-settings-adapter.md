---
id: BPR-0181
title: "Adapter: durable appearance settings behind appearance_settings_port"
status: Done
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

## What was built

- `src/adapters/sqlite/src/appearance_settings_database.{hpp,cpp}` —
  application id `0x42505241` ("BPRA"), schema version 1, one row.
- `include/bpr/adapters/sqlite/appearance_settings_store.hpp` and
  `src/appearance_settings_store.cpp` — `make_appearance_settings_store`.
- `tests/sqlite_contract/appearance_settings_storage_tests.cpp` — nine tests.

### The palette is stored as a theme document

Not as forty-five rows of colour components. `core::export_theme` writes it and
`core::import_theme` reads it back, which means the durable store **inherits
every refusal the importer already makes** — unknown keys, duplicate keys,
external references, include directives, style payloads, executable payloads,
alphabet violations, oversized documents, and any palette that fails to assign
every role.

Inventing a second encoding here would mean inventing a second set of those
refusals, and the second set is the one nobody tests against a hostile file.
RFC-BPR-0006 already claims `Theme import is safe` as verified; this makes the
durable read path the same code path that claim was proven against.

It also keeps the record keyed by each role's **stable identifier** rather than
its enum ordinal, so inserting a role into the vocabulary cannot silently
re-colour every role after it.

### The mode is decoded against the published vocabulary

`mode_of` compares the stored name against `core::stable_name` for every
declared mode rather than against a list restated here. A mode added to the
core is storable without editing this adapter, and a name the core does not
publish is refused rather than mapped to whatever sits at that ordinal.

### Absent stays distinguishable

`load` returns `std::optional`, and an absent record is reported absent. This
is the deliberate difference from BPR-0189's workbench layout store, whose port
returns the record directly. RFC-BPR-0006 requires a fresh installation to
resolve System, so telling a fresh installation from one that deliberately
stored System is load-bearing here and meaningless there.

## A test of mine that was wrong, and the adapter that caught it

The first version stored `application::appearance_settings{ }` as "the
defaults" and the adapter refused it. The adapter was right: a
default-constructed `font_settings` has an empty family, which the core rejects
on purpose — it cannot know a family and says so by being illegal until a host
supplies one.

So the test had been storing something no composition ever produces. It now
uses an explicit `legal_baseline()` and says why.

## Evidence

### Four breaks, each with 0 compile errors, each on its own assertion

| Break | Assertion that failed |
|---|---|
| Absent record answers the defaults | "a fresh installation reports no stored record at all" |
| Palette read back without the importer | "a palette the core's importer refuses is not adopted" |
| Unknown mode name mapped to System instead of refused | "a mode name the core does not publish is refused" |
| Core typography validation skipped on load | "a record the core's validator rejects is refused whole" |

The cancellation test carries the same shape BPR-0189 arrived at the hard way:
it establishes a record, attempts a cancelled store of a different one, and
asserts the stored record is **untouched**. Reporting cancellation after
writing satisfies a weaker assertion and is still wrong.

### Gates

- `ctest --preset dev` — **123/123**.
- `ctest --preset core-only` — **57/57**.
- `coding-format-check` passed. `verify_documentation.py` — `DOCUMENTATION_OK`.
