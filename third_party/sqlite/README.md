# Pinned SQLite amalgamation

BetterPullReview builds SQLite directly from the pinned upstream amalgamation
in this directory. The build has no system-library lookup or fallback.

Contents:

- `amalgamation/sqlite3.c` and `amalgamation/sqlite3.h` are the only upstream
  files required to build and consume the SQLite C library.
- `PROVENANCE.md` records the release, source identity, download, and hashes.
- `PUBLIC_DOMAIN.md` records SQLite's upstream public-domain dedication.
- `COMPILE_OPTIONS.md` records the fixed compile contract and the separate
  runtime configuration obligations of a catalog adapter.

The CMake target is the static library `better_pull_review_sqlite3`, exposed
to repository targets as `BetterPullReview::SQLite3`.

Do not patch the amalgamation in place. A dependency update replaces both
upstream files from one verified official archive and updates every recorded
version, source identity, and digest together.
