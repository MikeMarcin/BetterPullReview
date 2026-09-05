---
schema: "implement-rfc.card.v1"
id: "BPR-0189"
title: "Adapter: durable workbench layout settings"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0188"]
---

# Adapter: durable workbench layout settings

`workbench_application_host` takes a `workbench_settings_port&`. No
implementation exists anywhere in `src/` — the only files naming the port are
the application layer's own header and translation unit.

This is the clearest instance of the pattern BPR-0188 names: a port satisfied
by a fake never asks for an adapter. The port has been fully exercised by tests
since RFC-BPR-0005 landed, and has never had a real implementation, because
nothing ever needed one.

## Scope was narrowed after reading the ports

This card originally covered `workbench_observation_port` as well. Reading it
found that observation needs something that does not exist: a derivation from a
repository path to a `core::file_identity`. There is none in the codebase — the
only two constructions of that type are hard-coded byte fills inside
`github_application_fixture_access`. That derivation is shared with
RFC-BPR-0007's receipts, which are keyed by the same identity, so it is a core
decision rather than an adapter detail. **BPR-0192 owns it and the observation
adapter.**

## A correction to what this card first required

An earlier draft required that a store with no record be distinguishable from
one holding the defaults. That is the *appearance* port's contract, and it is
not this one's.

`appearance_settings_port::load` returns `std::optional<appearance_settings>`,
because RFC-BPR-0006 has a fresh-install-defaults-to-System criterion and
therefore has to tell an absent record from a deliberately stored one.

`workbench_settings_port::load` returns `workbench_layout_settings` directly,
and its own comment is explicit: "An absent durable record is not a failure: an
implementation with nothing stored returns the default settings." There is no
optional and no distinction to preserve. A default layout is a default layout,
and nothing in RFC-BPR-0005 asks how it came to be one.

Do not add the distinction back by inventing a sentinel. Two ports differing
deliberately is not an inconsistency to harmonise.

## What is stored

`workbench_layout_settings` entire: both drawers' semantic state and both
widths. Defaults are `expanded`, `expanded`, 320, 320.

A separate store from the epoch catalog, the review ledger, and the queue
enrollment database, for the reason those are already separate from each other:
deleting one kind of local state must not destroy another. Losing a drawer
width must not lose reading progress.

Follow the established SQLite conventions — `PRAGMA application_id`,
`PRAGMA user_version`, WAL, `STRICT` tables, forward-only migration.

## Required direct evidence

- With nothing stored, `load` answers the defaults and reports success. The
  test reads the defaults from `workbench_layout_settings`'s own declaration
  rather than restating 320.
- Settings round-trip whole: what `store` wrote is what `load` returns,
  compared as a whole record rather than field by field against a list somebody
  remembered to update.
- A width outside `[minimum_workbench_drawer_width,
  maximum_workbench_drawer_width]` is handled exactly as the port specifies,
  and the test reads those published bounds rather than restating them.
- A stored drawer state outside the core's closed vocabulary is refused rather
  than adopted, since a durable record can be written by another build or
  tampered with.
- A database stamped with a newer `user_version` fails closed, and the test
  asserts the **reason** it failed rather than only that it failed. Removing
  the guard must not still fail for an unrelated reason — this trap has been
  hit twice in this repository, most recently in BPR-0170.
- Cancellation through the supplied `std::stop_token` is honoured on both
  operations and reports `cancelled`, distinctly from `unavailable`.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

## What was built

- `src/adapters/sqlite/src/workbench_settings_database.{hpp,cpp}` — the store,
  application id `0x42505257` ("BPRW"), schema version 1.
- `src/adapters/sqlite/include/bpr/adapters/sqlite/workbench_settings_store.hpp`
  and `src/workbench_settings_store.cpp` — `make_workbench_settings_store`,
  returning a `workbench_settings_port`.
- `tests/sqlite_contract/workbench_settings_storage_tests.cpp` — eight tests.

### The layout is a singleton, structurally

`workbench_layout_settings` carries no host, account, repository, or pull
request, so drawer geometry is one reviewer preference rather than a per-review
fact. `singleton INTEGER PRIMARY KEY CHECK(singleton = 1)` is what makes a
second layout *unstorable* rather than merely unwritten. A reviewer should not
have to re-arrange their window for each pull request.

### The width bounds are interpolated, not restated

The `CHECK` constraints are built from
`application::minimum_workbench_drawer_width` and its maximum rather than
written out, so the bound the database enforces and the bound the application
publishes cannot drift apart. That is why `schema_version_1()` is a function
returning strings instead of the `constexpr std::array<std::string_view>` the
other stores use — a small ugliness bought deliberately.

### Defence in depth on the drawer state

`STRICT` plus the column `CHECK` refuses a bad value on write, and
`drawer_state_of` refuses one on read. Both are needed: the write guard cannot
help when the file was produced by another build or edited by hand, which is
exactly when a value outside the closed vocabulary shows up.

## The cancellation defect this card found

`progress_scope` installs a SQLite progress handler that fires every 256
virtual-machine steps. These statements finish in fewer, so **the handler never
runs and a pre-cancelled call is not stopped by it**. `execute()` already knew
this and checks `stop_requested()` before doing any work; the first draft of
this store went through `prepare` and `sqlite3_step` directly and so skipped
that guard entirely.

The failure mode is specific and nasty: with only a post-step check, a
cancelled `store` **performs the write and then reports cancellation**. The
caller is told their write did not happen while the bytes on disk say it did.

## A test gap the breaks exposed

The first version of the cancellation test asserted only that a cancelled store
returns a failure. Break 3 removed the pre-write guard and the test **still
passed**, because the post-step check catches the token afterwards and reports
cancellation — after writing.

So the test was measuring the wrong thing. It now establishes a layout first,
attempts a cancelled store of a different one, and asserts the stored layout is
**untouched**. That assertion is what makes the guard load-bearing, and it is
the one break 3 now fails.

Two earlier break attempts were also invalid and are recorded because the
lesson is the same one: one produced a compile error, so ctest ran a stale
binary and reported `Passed`, and one silently failed to match its target text.
A break is only evidence when the build succeeded *and* the edit applied. Both
were redone with the edit verified and the error count checked.

## Evidence

- `ctest --preset dev` — **120/120**, the new test included.
- `ctest --preset core-only` — **55/55**, configured and built fresh.
- Focused: `sqlite_workbench_settings_storage` — 8/8.
- `coding-format-check` — 280 files, passed.
- `verify_documentation.py` — `DOCUMENTATION_OK`. `git diff --check` clean.

The core-only run matters here for a specific reason: this slice adds a SQLite
adapter, and `core-only` links neither Qt nor SQLite. Its 55/55 says the new
store did not leak a storage dependency back across the core boundary.

### Five breaks, each landing on its own assertion, each with 0 compile errors

| Break | Assertion that failed |
|---|---|
| Forward-only version guard removed | "because this build will not read a schema it does not know, not because it tripped over its own CREATE" |
| Unknown drawer state cast to `expanded` instead of refused | "a drawer state outside the closed vocabulary is refused rather than cast into a state the core would be asked to reason about" |
| Pre-write cancellation guard removed | "and a cancelled store leaves the stored layout untouched rather than writing and then reporting cancellation" |
| Width `CHECK` stops tracking the published bounds | "a width one below the published minimum is refused by the schema" |
| Singleton primary key relaxed | "a second store leaves exactly one row, so no earlier layout can be restored by accident" |

The forward-only break is the one worth reading twice. Without the guard the
open **still fails** — `establish_schema` runs its first `CREATE` against a
table that already exists — so a test that stopped at "it failed" would pass
against a build with no forward-only policy at all. Asserting the *reason* is
the whole test. This trap has now been hit three times in this repository.
