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
- open only the fixed validated app-owned path with
  `SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX |
  SQLITE_OPEN_PRIVATECACHE | SQLITE_OPEN_NOFOLLOW`, never `SQLITE_OPEN_URI`;
- enable and verify `SQLITE_DBCONFIG_DEFENSIVE`, disable and verify trusted
  schema, and enable and verify foreign keys;
- select and verify local WAL mode and `synchronous=FULL`;
- use prepared allowlisted SQL and reject unexpected schema objects, versions,
  paths, permissions, integrity results, or database identities.

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
