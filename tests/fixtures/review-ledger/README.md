# Review ledger fixtures

## `v11-notes.sqlite`

A schema 11 review ledger written by the shipped code at `d6ccfb8b3`, before
RFC-BPR-0127 changed notes into Local comments. It is kept byte for byte so the
schema 12 migration is tested against a file the old build wrote, not against a
shape a test rebuilt. SHA-256
`364bcfd6f27afd84177fb8206e3be9b5abbc91c63215c12722e44cf2e1c1beeb`.

It was made by a temporary, uncommitted mode of
`tests/sqlite_contract/review_ledger_storage_tests.cpp`. The mode stored two notes,
moved the file back to schema 6 with `restore_v6_fixture`, reopened it so the
shipped migrations took it to schema 11, stored the rest through
`review_ledger_database::store_note` and `erase_note`, then checkpointed it.
`PRAGMA journal_mode=DELETE; VACUUM` was run once afterwards.

Scope 1 (`github.com`, `octo`, `app`, pull request 7):

| Note | Written | Anchor | Text |
|---|---|---|---|
| 1 | schema 6 | file (seed 10), no path, no line | `schema 6 whole-file note, no path` |
| 2 | schema 6 | the pull request | `schema 6 note on the pull request` |
| 3 | schema 11 | file (seed 20), path `src/engine/renderer.cpp`, no line | UTF-8 with an em dash, a check mark, two trailing spaces, a newline and a tab |
| 4 | schema 11, edited twice (revision 6) | file (seed 30), path `src/net/retry.cpp`, right line 41, SHA-1 blob `4041…53` | `the budget resets here, edited twice` |
| 5 | schema 11 | file (seed 40), path `src/net/retry.cpp`, left line 0, no blob | `base side, no blob kept` |
| 6 | deleted | | |
| 7 | schema 11 | file (seed 60), path `docs/large.md`, no line | 16,384 bytes of `x` (the note bound) |

Scope 2 (pull request 8): note 1, file (seed 70), path `README.md`, right line 0
with the same blob, `the other review's note one`.

The file and comparison identities are `opaque<…>(seed)` and
`opaque<…>(seed + 1)` as `review_ledger_storage_tests.cpp` builds them.
