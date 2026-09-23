---
schema: "implement-rfc.card.v1"
id: "BPR-0887"
title: "Nested progress scopes reset the handler to null, silently discarding the store-level step ceiling"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable sqlite partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0887: Nested progress scopes reset the handler to null, silently discarding the store-level step ceiling

## Defect

`binding_read_scope`, `registry_read_scope` and `bounded_read_scope` install a
step-bounded SQLite progress handler so an untrusted read cannot run away. But
every nested `detail::execute` installs its **own** `progress_scope`, and that
destructor resets the handler to **null** rather than restoring the previous
one (`src/adapters/sqlite/src/sqlite_connection.cpp:17-27`).

So after the first `execute` inside `establish` — `configure_connection`, for
instance — the ceiling is gone. The comment at
`src/adapters/sqlite/src/device_binding_store.cpp:55-56` says the bound "must
precede all untrusted reads"; from the second statement onward it does not
apply to any of them.

This is a bound that silently stops bounding, which is the failure mode a
bound exists to prevent. Nothing reports it and no test would notice, because
the handler's absence only shows up under a read that actually needs
stopping.

## Scope

- Make `progress_scope`'s destructor **restore the previous handler** rather
  than clearing it, so nesting composes. That is the smaller and more general
  fix, and it is why this is filed as a defect rather than a rearrangement of
  the call sites.
- Alternatively, install the store-level scope after `configure_connection` —
  but that only fixes the sites someone remembers, so prefer the first.
- Add a check that a nested `execute` leaves the outer ceiling in force. A
  bound with no test is how this happened.
- This was offered as a quick fix and is deliberately **not** being taken as
  one in this dispatch: it changes RAII semantics used across the SQLite
  adapter, and the safe version needs the test above plus a run of the SQLite
  contract suite. It is small, not trivial.

## Acceptance

- After a nested `execute` returns, the outer step ceiling is still installed,
  proven by a test.
- The SQLite contract suite passes on all three platforms.
- The comment at `device_binding_store.cpp:55-56` is true again.

## Every nested scope hands the connection back (bpr-main-mac, Mac batch 5, 2026-09-23)

Station `bpr-main-mac` (Apple Silicon, AppleClang, Qt 6.11.2, `build/dev`
Debug), claimed at `7eb4aa87`, candidate built on `ac1c6cdf`.

**Re-check at HEAD: the defect holds, and it is wider than the card says.**
`progress_scope`'s destructor still clears the handler
(`sqlite_connection.cpp`), and `configure_connection` still runs `execute`
inside `binding_read_scope`, so every statement after it ran with no handler.
The card names three store scopes. There are six installers, and each one
cleared the handler on exit:

- `progress_scope` (`sqlite_connection.cpp`), the one inside every `execute`;
- `binding_read_scope` (`device_binding_store.cpp`), `registry_read_scope`
  (`profile_registry_store.cpp`) and the two `bounded_read_scope`s
  (`window_review_restoration_store.cpp`, `workbench_settings_database.cpp`);
- `progress_guard` (`epoch_catalog.cpp`). It does not nest today.

The loss also ran the other way. `sqlite_profile_registry_store::save` opens a
`progress_scope` for cancellation and then calls `read_snapshot`, whose
`registry_read_scope` cleared the handler when it ended. So the deletes and
inserts that follow ran without any cancellation check.

**Shape: one scope class, and the innermost scope governs.** SQLite keeps one
progress handler per connection and has no call that reports the installed
one. So the only way to restore it is for every installer to record what it
installs. `detail::progress_handler_scope` (`sqlite_connection.hpp`) is now the
only code that calls `sqlite3_progress_handler`. It installs its handler and
records itself as the calling thread's innermost scope, linked to the scope it
enclosed. When it ends, it walks that chain to the nearest enclosing scope on
the **same connection** and reinstates that scope's handler, context and
interval. If there is none, it clears the handler. `progress_scope` and the
five other classes now hold one as a member, so none of them has a destructor
of its own. Their ceilings, intervals and callbacks are unchanged.

Two choices in that shape:

- **The innermost scope governs alone.** An enclosing ceiling is reinstated
  when the nested scope ends, but it does not also run inside it. The reason
  is `establish`'s `execute(Database, "ROLLBACK")`, which has no stop token on
  purpose. Chaining the handlers would let a spent ceiling interrupt that
  rollback.
- **The record is thread-local, not SQLite client data.** RAII scopes nest on
  the thread that runs them, and the record costs nothing and cannot fail.
  `sqlite3_set_clientdata` would put the record on the connection, but it
  allocates for every outermost scope, including the one inside every
  top-level `execute`. If that allocation failed, a nested scope could not
  find the one it enclosed, and the ceiling would lapse just as before.

The first draft used a function-local `thread_local`. clang-tidy's
`clang-analyzer-core.StackAddressEscape` flagged each `return` in `execute`
(five findings), saying the static still held the scope's address. That is a
false positive. The destructor restores the record before the scope's storage
ends, and the analyzer does not model that. The candidate declares the record
at namespace scope, which that checker does not track. The nesting checks
below are what show the record is right.

**Limits are a separate matter.** `configure_connection` still replaces the
binding scope's `sqlite3_limit` values with the project's connection policy,
as it did before. The reads after it are the scope's own `read_rows` (`LIMIT
65`), over rows the scope has already validated or just written. This change
does not touch limits. The comment at `device_binding_store.cpp:55-59` now
says both things: the step ceiling holds for the whole scope, and the limits
are replaced.

### The check (AC1)

`nested_progress_scopes()` in `tests/sqlite_contract/device_binding_storage_tests.cpp`
(row `sqlite_device_binding_storage`) runs on two in-memory connections, after
the forked phases, because the parent initializes SQLite only once its
children exit. It detects a handler by its effect. A recursive count to
100,000 needs far more steps than any ceiling in the test, so it completes
with no handler and returns `SQLITE_INTERRUPT` under one. In order:

1. No handler: the long query completes.
2. An outer `progress_handler_scope` with a 50-call ceiling: a short query
   counts against it and does not reach it.
3. A nested `detail::execute("CREATE TABLE t(x)")` succeeds, and the outer
   count does not move while it runs.
4. **After it returns, the long query is interrupted, with the outer count at
   exactly 50. This is the acceptance check.**
5. With the ceiling spent, a nested `execute` of a 100,000-row `CREATE TABLE
   AS` with no stop token still succeeds. This is the `ROLLBACK` case.
6. A scope on the second connection, with a `progress_scope` on the first
   nested inside it. The inner scope governs the first connection. When it
   ends, the first connection's outer ceiling is back, and the second
   connection's own ceiling still works.
7. When the second connection's scope ends, the first connection's ceiling is
   untouched, and the second connection has no handler left.
8. When the outer scope ends, the first connection has no handler left.

**Negative controls**, each in a temporary build of the final candidate
(`sqlite_connection.cpp` SHA-256 `2630ff00…2b0d64`), restored and re-hashed
afterwards (`grep -c BPR0887` gives 0 in `src` and `tests`):

| Break | Result |
| --- | --- |
| The destructor clears the handler, as before | Fails at step 4: `FAILED: the outer ceiling is still installed after a nested execute returns, with its count intact` |
| The destructor reinstates the enclosing scope on any connection | Fails at step 6: `FAILED: a scope on another connection keeps its own ceiling` |

The same two breaks gave the same two failures on the function-local draft.

### Headroom: statements after a nested execute now count

Every statement that used to run after a nested `execute` with no handler now
counts against its store's ceiling. So a legitimate operation could newly hit
one. A temporary diagnostic build printed each bounded scope's final
callback count when it ended. The 41-row serial run below passed with it, and
the diagnostic was then removed (the four store files re-hashed to the
candidate):

| Scope | Most callbacks seen | Ceiling | Scopes seen |
| --- | --- | --- | --- |
| `binding_read_scope` | 31 | 200 | 47 |
| `registry_read_scope` | 147 | 2,048 (`MaximumRetainedWindows`) | 569 |
| `bounded_read_scope`, window restoration | 3 | 200 | 644 |
| `bounded_read_scope`, workbench settings | 8 | 400 | 770 |

The 41 rows were the SQLite suite, every `application_profile_*` and
`application_device_binding_*` row, `application_window_review_restoration`,
`application_workbench_preferences_consumers`,
`application_appearance_profile_consumers`,
`profile_window_restoration_fixture_contract` and
`application_workbench_composition_contract`. All passed, in 845 s
(`application_profile_window_review_restoration_public_path` took 563 s of
that). That run was on the function-local draft. The final candidate differs
only in where the thread-local pointer is declared.

### Verification at the candidate (macOS)

| Check | Result |
| --- | --- |
| `cmake --build --preset dev` | Exit 0, no `warning:` line |
| SQLite contract suite: `ctest --test-dir build/dev -j1 --output-on-failure -R '^(sqlite_\|contained_catalog_probe$)'` | 18/18 passed, on the draft and again on the final candidate |
| Both negative controls | Fail as described above |
| `architecture_boundaries`, `architecture_boundary_contract`, `core_boundary_contract`, `conditional_branch_identifier_contract`, `test_network_isolation_manifest` | 5/5 passed |
| clang-format 23.1.1 on the eight files | Clean. One realignment in `device_binding_store.cpp` was applied |
| `python3 _coding-standard/scripts/tidy.py --build-dir build/dev --paths` on the seven translation units | No finding on a changed line. The one warning is in `home_view_state.hpp:35`, and it is the same at HEAD |

### Still needed: the Linux and Windows halves (AC2)

The acceptance asks for the SQLite contract suite on all three platforms, so
the card stays In Progress. The change is portable C++. Nothing in it depends
on the platform except that `test_device_binding_storage` forks on POSIX and
runs in-process on Windows, and the new check runs after either.

- **Linux (bpr-main-linux):**

  ```sh
  cmake --build --preset dev
  ctest --test-dir build/dev -j1 --output-on-failure -R '^(sqlite_|contained_catalog_probe$)'
  ```

  Expected: 18/18, including `sqlite_device_binding_storage` with
  `device-binding-storage-tests:pass`.
- **Windows (bpr-main-win):**

  ```sh
  cmake --build --preset windows-dev
  ctest --preset windows-dev -j1 --output-on-failure -R '^(sqlite_|contained_catalog_probe$)'
  ```

  Expected: the same rows plus the two Windows-only rows,
  `sqlite_appcontainer_private_descriptor` and `sqlite_private_store_open`.

When both pass on a head that contains this change, the card can finish.

### Acceptance state

- **After a nested `execute`, the outer ceiling is still installed, proven by
  a test:** pass on macOS (steps 3 and 4 above, and the first negative
  control).
- **The SQLite contract suite passes on all three platforms:** macOS passes.
  **Linux and Windows are owed.**
- **The comment at `device_binding_store.cpp:55-56` is true again:** pass. The
  ceiling now holds for the whole scope, and the comment (now lines 55-59)
  also says that `configure_connection` replaces the limits.
