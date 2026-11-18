---
schema: "implement-rfc.card.v1"
id: "BPR-2254"
title: "A profile from build 63 or earlier loses every saved agent in build 66, because the agent store's ADD COLUMN migration always fails on the hardened connection"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P0"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2254--BPR-2254--c981eb124d75.json","found_by":"owner, 2026-11-18: after updating from build 63 to 0.5.0 build 66, the one custom agent showed 'Not found' and every check failed with '<agent> isn't where it was found any more'. After a restart the entry was gone from Settings → Agents","related_cards":["BPR-1602","BPR-1831","BPR-1859","BPR-2051","BPR-2149","BPR-2152","BPR-2181","BPR-2241"],"related_rfcs":["RFC-BPR-0125","RFC-BPR-0126"]}
acceptance: []
---

# BPR-2254: A profile from build 63 or earlier loses every saved agent in build 66, because the agent store's ADD COLUMN migration always fails on the hardened connection

## Observed

The owner, 2026-11-18, on macOS, updating from build 63 to build 66 (builds
64 and 65 were never run on this profile). The profile had one custom agent:
Claude stream JSON, its program an absolute path to a wrapper executable.

- Settings → Agents showed the entry as **Not found**. Check, the entry's
  own Check and Check all each failed at once with "<agent> isn't where it was
  found any more. Look again, or choose its program in Settings." Show details
  said "No process output was returned."
- The program was present, a regular file, and executable for the user.
- After a restart the entry was gone from the list. Re-adding it made it
  appear again, but its check still failed the same way.

The session log for every build 66 launch shows the store refusing at once,
which build 63 never logged:

```text
storage.database.succeeded action=storage_open  store=agent_setup
storage.database.failed    action=storage_statement failure=sql_error store=agent_setup
```

Every check logged `action=agent_status_check failure=agent_not_found
process=none duration_ms=0`. Nothing was inspected or launched. Under build
63, the same entry's checks logged `action=agent_custom_check failure=none`.

### Reproduced

The installed build 66 was run three times, each in its own scratch home
(`CFFIXED_USER_HOME`), with a copy of the owner's `agent-setup.sqlite3`:

| Run | Store at launch | Result |
|---|---|---|
| A | The copy, unchanged (`native_agent_turns` without `seconds`, 16 rows) | `sql_error`; `seconds` and `prompt_macro_packs` still missing afterwards |
| B | The same copy with `seconds` added by hand | Opens; creates `prompt_macro_packs` itself; the agent entry is intact |
| C | No file | Opens |

A standalone program built from the pinned SQLite amalgamation (3.53.4)
narrows it to one prepare flag:

| Statement on a `STRICT` table | Prepared with `PERSISTENT` | With `PERSISTENT \| NO_VTAB` |
|---|---|---|
| `ADD COLUMN b INTEGER` | ok | `no such table: pragma_quick_check` |
| `ADD COLUMN b INTEGER NOT NULL DEFAULT 0` | ok | `no such table: pragma_quick_check` |
| `ADD COLUMN b INTEGER CHECK(b IS NULL OR b >= 0)` | ok | `no such table: pragma_quick_check` |

The result is the same with rows in the table and with none.

## Cause

1. `detail::prepare` (`src/adapters/sqlite/src/sqlite_connection.cpp`)
   prepares every statement with
   `SQLITE_PREPARE_PERSISTENT | SQLITE_PREPARE_NO_VTAB`.
   `device_binding_store.cpp` and `epoch_catalog_support.cpp` do the same.
2. SQLite carries out `ALTER TABLE … ADD COLUMN` on a `STRICT` table by
   running a nested query over the `pragma_quick_check` table-valued
   function, which is a virtual table. The nested parse inherits `NO_VTAB`, so
   on this connection **every** `ADD COLUMN` on a `STRICT` table fails, for
   any column definition and any row count.
3. `agent_setup_store.cpp` has three such migrations:
   - `add_native_turn_seconds_column` ([[BPR-2241]], first shipped in build
     64): `native_agent_turns.seconds`;
   - `add_queue_prompt_column` ([[BPR-2181]] AC13):
     `agent_queue_entries.prompt`;
   - `add_queue_claim_column` ([[BPR-2152]]): `agent_queue_entries.claim`.

   Each runs only when the table exists without the column, so a new profile
   never reaches it: `CREATE TABLE IF NOT EXISTS` already has the column.
   The owner's queue tables happened to be created after both queue columns
   existed, so only `seconds` failed for them. A profile whose queue tables
   predate either column fails the same way.
4. The store's open is one `||` chain that returns an empty connection on the
   first failure. Every step before it commits on its own, so the file is left
   part-migrated: it has the earlier tables, but not `seconds` or the
   `prompt_macro_packs` table ([[BPR-2149]]) after it. The next launch fails
   at the same step.
5. With no store, the app runs on default agent preferences. The list is
   empty, there is no preferred entry, and detection finds no saved program.
   `chosen_row()` is null, so `start_check` reports `agent_not_found` without
   inspecting anything (`agent_setup_host.cpp`, the [[BPR-2051]] and
   [[BPR-1859]] branches). Writes go nowhere, so a re-added entry lasts only
   until the app quits.
6. This was found once before and not carried over. The appearance store's
   `migrate_to_version_3` (`appearance_settings_database.cpp`) says:

   ```cpp
   // Rebuilt rather than altered. ALTER TABLE ADD COLUMN on a STRICT
   // table runs SQLite's quick check through the pragma table-valued
   // function, and the hardened connection policy refuses that
   // function, so the row is carried across a fresh current-format table.
   ```

7. No contract test opens an agent store written by an earlier build, so no
   test ever ran these migrations on the real connection policy.

## Change

### 1. Rebuild instead of altering

- Replace all three `ADD COLUMN` migrations with a table rebuild in one
  `BEGIN IMMEDIATE` … `COMMIT`: create the current-format table under a new
  name, `INSERT … SELECT` the old columns (the new column takes its default
  or `NULL`), drop the old table, rename the new one into place. On any
  failure, `ROLLBACK` and leave the file as it was.
- The store's own `narrow_` rebuilds keep the copy simple with
  `INSERT INTO t SELECT * FROM narrow_t`. These rebuilds add a column, so
  name the copied columns explicitly.
- Mind foreign keys. `native_agent_turns` references `native_agent_sessions`,
  and `agent_queue_entries` references `agent_queues`. Check whether any
  table references the one being rebuilt. If one does, use SQLite's
  documented procedure for other schema changes ("Making Other Kinds Of
  Table Schema Changes": foreign keys off, rebuild,
  `PRAGMA foreign_key_check`, foreign keys back on). The connection policy
  verifies `foreign_keys` is on afterwards.
- Leave `NO_VTAB` as it is. The flag is deliberate hardening, and a rebuild
  needs no virtual table.

### 2. Guard every store against it

- Add an `architecture_boundaries` (or `coding-tidy`) rule that refuses the
  text `ADD COLUMN` in `src/adapters/sqlite/src/`, with the reason, so the
  next migration cannot repeat this. If some store is ever opened without
  `NO_VTAB`, give it a named exemption.
- Move the explanation from the appearance store's comment to the connection
  policy in `sqlite_connection.cpp`, where every store author will see it,
  and point both comments at it.

### 3. Do not present a failed store as empty settings

- When the agent store fails to open, Settings → Agents and the agent picker
  say that the saved agent settings could not be read. They name the store
  file and offer the existing recovery path ([[BPR-1602]]). They do not show
  an empty list that looks like a fresh profile.
- While the store is unavailable, adding, editing or removing an agent is
  refused with that reason. It must not appear to succeed and then vanish on
  restart.
- A check while no row exists because the store failed does not report
  `agent_not_found` with "No process output was returned". It reports the
  store failure.

### 4. Tests

- `sqlite_contract` agent setup storage, using the store's real open (the
  hardened connection):
  - Seed a file with build 63's schema for `native_agent_turns` (no
    `seconds`) holding rows, plus saved agent entries and a preferred entry.
    Opening succeeds, the turns keep their rows with `seconds` `NULL`,
    `prompt_macro_packs` exists, and the entries and preferred entry read
    back unchanged.
  - The same for `agent_queue_entries` without `prompt`, and without `claim`,
    each holding a queued entry that reads back.
  - A file left part-migrated by build 64 to 66 (the earlier tables present,
    `seconds` and `prompt_macro_packs` missing) opens and completes.
  - A rebuild that fails midway (for example, a forced `INSERT` failure)
    rolls back, leaving the original table and rows intact.
- A connection-level test: on the hardened connection, `ADD COLUMN` on a
  `STRICT` table fails. This pins the reason for rule 2, and shows when a
  SQLite update changes that behaviour.
- Application or Qt test: with an agent store that fails to open, Settings
  shows the store failure rather than an empty list, and adding an agent is
  refused with that reason.

### 5. Documentation

- Release notes: users updating from build 63 or earlier, who had used an
  agent in a pull request, may have lost their agent list in builds 64 to 66.
  The fixed build restores it from the same file. Nothing was deleted,
  because the store never opened to write.
- Note the rebuild rule wherever this repository's SQLite store conventions
  are described.

## Workaround until this ships

Quit BPR completely. In a terminal, back up the file and add the missing
column with any `sqlite3` (the system one opens without `NO_VTAB`):

```sh
cd ~/Library/Application\ Support/BetterPullReview/BetterPullReview/state/v1   # macOS default profile
cp agent-setup.sqlite3 agent-setup.sqlite3.backup
sqlite3 agent-setup.sqlite3 "ALTER TABLE native_agent_turns ADD COLUMN seconds INTEGER CHECK(seconds IS NULL OR seconds BETWEEN 0 AND 4294967295)"
```

Then start BPR again. The store opens, finishes its own remaining
migrations, and the saved agents come back. If the queue tables predate their
columns, the log will show the store still failing. In that case, add those
columns the same way, using the definitions in `add_queue_prompt_column` and
`add_queue_claim_column`.

## Acceptance

- AC1. A profile whose agent store was written by build 63 or earlier opens
  in the fixed build with every saved agent entry, the preferred entry,
  conversation turns and queued entries intact, and a custom agent's check
  runs its program again.
- AC2. A store left part-migrated by builds 64 to 66 opens and completes its
  migration.
- AC3. No store under `src/adapters/sqlite/` uses `ALTER TABLE … ADD COLUMN`,
  and a gate refuses it with the reason.
- AC4. A rebuild migration that fails leaves the file exactly as it was.
- AC5. When the agent store cannot be opened, the app says so where agents
  are shown and refuses agent edits with that reason. It never shows an empty
  list or a "not found" check in its place.
- AC6. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Out of scope

- Relaxing `SQLITE_PREPARE_NO_VTAB` for migrations. A rebuild needs no
  virtual table, and narrowing the hardening for every store is a larger
  decision.
- A general schema-version framework for the agent store, which today detects
  each migration by looking for text in `sqlite_schema`. That is worth doing,
  but it is not needed for this fix.
