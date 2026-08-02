# Pinned SQLite source provider

BetterPullReview downloads the pinned upstream amalgamation through the
verified CPM source cache. It stages only the verified `sqlite3.c` and
`sqlite3.h` into the build tree and builds from that controlled directory. The
build has no system-library lookup or fallback, and unrelated cache files never
enter its include path.

Contents:

- `cmake/BprSQLiteProvider.cmake` owns download-only acquisition, archive and
  extracted-file hashes, `SQLITE_VERSION`, `SQLITE_SOURCE_ID` validation, and
  controlled two-file staging.
- `PROVENANCE.md` records the release, source identity, download, and hashes.
- `PUBLIC_DOMAIN.md` records SQLite's upstream public-domain dedication.
- `COMPILE_OPTIONS.md` records the fixed compile contract and the separate
  runtime configuration obligations of a catalog adapter.

The CMake target is the static library `better_pull_review_sqlite3`, exposed
to repository targets as `BetterPullReview::SQLite3`.

Do not patch cached source. A dependency update changes the pinned archive,
version, source identity, and every digest together, then repopulates a clean
CPM cache entry.
