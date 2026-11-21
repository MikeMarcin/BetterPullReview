# SQLite provenance

| Fact | Value |
|---|---|
| Release | SQLite 3.54.1 |
| Release date | 2026-10-16 |
| Amalgamation archive | `sqlite-amalgamation-3540100.zip` |
| Official archive URL | `https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip` |
| Official download record | `https://www.sqlite.org/download.html` |
| Official release record | `https://www.sqlite.org/changes.html` |
| `SQLITE_SOURCE_ID` | `2026-10-16 14:45:20 555e07ef33f69a0957d24d8e555b702464a131d6f1607f4c3c1e4551e8e6ca76` |
| Archive SHA3-256 | `92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30` |
| `sqlite3.c` SHA3-256 | `e2b869d3003b33c942b7dd7063604fbc53553e1b66609570df39b9b7b558ee81` |
| `sqlite3.h` SHA3-256 | `15ef3f5dbc4a7a980f828fa0d1e473cee5a124e7bb77e9bc70fc0bfa3f9a2af8` |

The archive digest and `sqlite3.c` digest are published by sqlite.org. The
`sqlite3.h` digest is an additional repository intake measurement of the same
verified archive.

The build downloads this archive without adding its upstream CMake project,
then verifies `sqlite3.c` and `sqlite3.h` byte-for-byte in the CPM source cache
before copying exactly those two files to a controlled build-tree staging
directory and declaring `BetterPullReview::SQLite3`. The cache extraction
directory is never a compiler include path. No amalgamation source is tracked
in this repository.

The official ZIP also contains `shell.c` and `sqlite3ext.h`. They are not
compiled: BetterPullReview builds no SQLite command-line shell and omits
loadable extensions, so neither file is required by the static library or its
consumers.

## Reproducible intake

Run from a private temporary directory to independently reproduce the provider
checks before changing a pin:

```sh
curl --proto '=https' --tlsv1.2 --fail --location \
  --output sqlite-amalgamation-3540100.zip \
  https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
openssl dgst -sha3-256 sqlite-amalgamation-3540100.zip
unzip sqlite-amalgamation-3540100.zip
openssl dgst -sha3-256 \
  sqlite-amalgamation-3540100/sqlite3.c \
  sqlite-amalgamation-3540100/sqlite3.h
```

Reject a download if either upstream-published digest differs. Update
`cmake/BprSQLiteProvider.cmake` and this record together; do not carry generated
tools, extension headers, archive metadata, or temporary download artifacts
into the repository.
