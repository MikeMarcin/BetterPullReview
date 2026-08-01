# SQLite compile contract

`BetterPullReview::SQLite3` is one pinned static C library. Its compile
definitions are part of the dependency contract and are exported to consumers
so that `sqlite3.h` exposes the same feature surface as the library.

| Definition | Contract |
|---|---|
| `SQLITE_THREADSAFE=1` | Build serialized threading support; catalog connections must still request `SQLITE_OPEN_FULLMUTEX`. |
| `SQLITE_DQS=0` | Disable double-quoted string-literal compatibility. |
| `SQLITE_OMIT_LOAD_EXTENSION=1` | Remove loadable-extension support. |
| `SQLITE_OMIT_SHARED_CACHE=1` | Remove process shared-cache support. |
| `SQLITE_OMIT_AUTOINIT=1` | Remove implicit global initialization; the owning adapter must call `sqlite3_initialize()` before any other SQLite API. |
| `SQLITE_USE_URI=0` | Keep URI filename interpretation off by default; callers must not pass `SQLITE_OPEN_URI`. |
| `SQLITE_MAX_ATTACHED=0` | Give the dedicated catalog zero attachment capacity; every `ATTACH` fails before another database can open. |
| `SQLITE_MAX_PAGE_COUNT=262144` | Cap the compile-time page-count ceiling at 262,144; with the required 4,096-byte catalog page size, the main database is bounded to 1 GiB. |
| `SQLITE_DEFAULT_FILE_PERMISSIONS=0600` | Create SQLite-owned files with owner-only permissions on Unix-like targets. |
| `SQLITE_DEFAULT_FOREIGN_KEYS=1` | Make foreign-key enforcement fail-safe by default; the adapter must also enable and verify it per connection. |
| `SQLITE_DEFAULT_SYNCHRONOUS=2` | Keep the non-WAL default at `FULL`. |
| `SQLITE_DEFAULT_WAL_SYNCHRONOUS=2` | Keep WAL synchronization at `FULL`, rather than SQLite's performance-oriented `NORMAL` recommendation. |
| `SQLITE_TEMP_STORE=3` | Keep SQLite temporary tables and indices in memory; the adapter must bound all inputs and queries. |
| `SQLITE_TRUSTED_SCHEMA=0` | Make trusted schema off by default; the adapter must also disable and verify it per connection. |
| `SQLITE_ENABLE_API_ARMOR=1` | Add SQLite's second-line API-misuse checks; application correctness must not depend on `SQLITE_MISUSE`. |
| `SQLITE_OMIT_DEPRECATED=1` | Remove SQLite's deprecated C interfaces and legacy pragmas from the pinned API surface. |

These definitions do not substitute for connection hardening. The catalog
adapter remains responsible for all runtime policy:

- call `sqlite3_initialize()` explicitly before any SQLite API and pair final
  process-wide teardown with `sqlite3_shutdown()` only after all handles close;
- securely pre-create a private bootstrap database, validate and checkpoint it,
  and publish it without replacement; ordinary main-database opens use
  `SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX |
  SQLITE_OPEN_PRIVATECACHE | SQLITE_OPEN_NOFOLLOW | SQLITE_OPEN_EXRESCODE`,
  without `SQLITE_OPEN_CREATE` or `SQLITE_OPEN_URI`;
- enable and verify `SQLITE_DBCONFIG_DEFENSIVE`, foreign keys, and the
  query-planner stability guarantee; disable and verify trusted schema,
  double-quoted compatibility, views, triggers, writable schema, and SQLite
  worker threads;
- require a 4,096-byte page size and exact 262,144-page limit, disable memory
  mapping, select and verify local WAL mode and `synchronous=FULL`, and set the
  automatic-checkpoint threshold to 256 pages. The threshold triggers
  checkpoint attempts; it is not a hard WAL-file-size limit;
- set a 2-second busy timeout and exact connection limits: 1 MiB values,
  256 KiB SQL, 128 columns, expression depth 64, eight compound-select terms,
  250,000 virtual-machine operations, 32 function arguments, zero attached
  databases, 256-byte `LIKE` patterns, 128 variables, zero trigger depth, and
  zero worker threads;
- prepare only fixed SQL with `SQLITE_PREPARE_PERSISTENT |
  SQLITE_PREPARE_NO_VTAB`, install a runtime authorizer for the exact 18 BPEC
  tables and required functions/actions, and reject unexpected schema objects,
  versions, paths, permissions, integrity results, or database identities;
- compare every one of the 18 tables and 15 indexes by type, name, and
  canonical SQL, then require `integrity_check(1)` and an empty
  `foreign_key_check` before installing the runtime authorizer; and
- classify `SQLITE_FULL` as the typed catalog `quota_exceeded` failure.

The compile contract intentionally retains the progress-handler API for
bounded cancellation. Defensive mode remains a connection setting. The
trusted-schema compile default is defense in depth and does not replace the
adapter's per-connection setting and verification.

`SQLITE_OMIT_ATTACH` is intentionally not used with the official pre-generated
3.53.4 amalgamation. A direct target-native probe found that option excludes
the attachment implementation while the generated parser still references
internal attachment and vacuum functions, leaving unresolved symbols in the
static archive. `SQLITE_MAX_ATTACHED=0` keeps the exact upstream amalgamation
link-complete and directly enforces the required zero-attachment capacity. The
adapter must still use allowlisted SQL and reject `ATTACH` and `DETACH`.
