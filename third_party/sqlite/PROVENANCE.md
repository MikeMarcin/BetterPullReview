# SQLite provenance

| Fact | Value |
|---|---|
| Release | SQLite 3.53.4 |
| Release date | 2026-07-24 |
| Amalgamation archive | `sqlite-amalgamation-3530400.zip` |
| Official archive URL | `https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip` |
| Official download record | `https://www.sqlite.org/download.html` |
| Official release record | `https://www.sqlite.org/changes.html` |
| `SQLITE_SOURCE_ID` | `2026-07-24 19:02:57 bf7c7f30031888f4e796e429ab3978879485813aaca6f641c7b33e4e09459bcc` |
| Archive SHA3-256 | `628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e` |
| `sqlite3.c` SHA3-256 | `67f423e9ebbbdc473cbc4772c872ee6b89f31fde4ed0279a5c25d5f65c043a16` |
| `sqlite3.h` SHA3-256 | `89d0de498b1012938bb78dc18b68fea52e26f7f7b6db6347d126a2e58b86a05c` |

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
  --output sqlite-amalgamation-3530400.zip \
  https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip
openssl dgst -sha3-256 sqlite-amalgamation-3530400.zip
unzip sqlite-amalgamation-3530400.zip
openssl dgst -sha3-256 \
  sqlite-amalgamation-3530400/sqlite3.c \
  sqlite-amalgamation-3530400/sqlite3.h
```

Reject a download if either upstream-published digest differs. Update
`cmake/BprSQLiteProvider.cmake` and this record together; do not carry generated
tools, extension headers, archive metadata, or temporary download artifacts
into the repository.
