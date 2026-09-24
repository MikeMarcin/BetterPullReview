---
schema: "implement-rfc.card.v1"
id: "BPR-1047"
title: "sqlite_review_ledger_storage leaves two bpr-ledger-preflight-* directories in the system temporary directory after every run"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-main-linux, BPR-0883's runs","confirmed_at":"78c8ac85","introduced_by":"38c10c33 (BPR-0250)","related_cards":["BPR-0250","BPR-0883"],"run":"bpr-main-linux-20260923","reservation":"docs/issues/_reservations/BPR-1046--BPR-1047--4cc848046b0e.json"}
---

# BPR-1047: sqlite_review_ledger_storage leaves two bpr-ledger-preflight-* directories in the system temporary directory after every run

## Defect

Each run of `sqlite_review_ledger_storage` leaves two directories named
`bpr-ledger-preflight-<32 hex>` in the system temporary directory. Nothing
removes them, so they build up one pair per run. This is older than
[[BPR-0883]]. The test that provokes them arrived with [[BPR-0250]]
(`38c10c33`, 2026-08-10).

The reserved title did not say where the directories go. This title adds
that: they land in the system temporary directory, not in the test's
scratch directory.

## Confirmed at `78c8ac85` (bpr-main-linux, 2026-09-23)

WSL2 Ubuntu 26.04, GCC 15.2, `build/dev` (Debug). The row ran once through
`ctest -j1` with fresh `HOME` and XDG roots, and with `TMPDIR` pointed at a
new empty owner-private directory. It passed in 1.72 s. Afterwards that
directory held exactly two entries:

| Directory | Contents | What made it |
| --- | --- | --- |
| `bpr-ledger-preflight-6ade…` | `clone/substitute-sentinel`; `clone.held/` with `ledger.db` (0 bytes), `ledger.db-wal` (0 bytes), `ledger.db-shm` (32 KiB) and `.identity` | the `substitute` mode |
| `bpr-ledger-preflight-1a7e…` | `clone/foreign-child` | the `nonempty` mode |

Both directories are mode `0700`. Neither contains the test's private
canary `BPR_PREFLIGHT_PRIVATE_CLONE_0250`; a search of every file returned
nothing. The leftovers are litter, not a leak of private data.

The shared `/tmp` on this station already held two more pairs of the same
shape from earlier runs that day (09:55). This card does not remove them.

## Cause

`make_preflight_directory` (`src/adapters/sqlite/src/review_ledger_database.cpp`)
creates its directory under `std::filesystem::temp_directory_path()`, which
is the system temporary directory (`TMPDIR` on POSIX, `%TEMP%` on Windows).
It is the only `temp_directory_path` call in `src/`.

`test_preflight_cleanup_preserves_substitutes_and_nonempty_directories`
(`tests/sqlite_contract/review_ledger_storage_tests.cpp`) installs a cleanup
hook that makes cleanup unsafe, in two ways:

- **`substitute`**: it renames the real clone to `clone.held` and puts a
  different directory at `clone`.
- **`nonempty`**: it puts an unknown file in `clone`.

The product then refuses to delete what it cannot prove it owns, and returns
`unavailable`. That refusal is correct, and it is what the test asserts. But
the test never removes the directory it caused to be kept. Its
`scratch_directory` holds only the ledger, not the preflight directory.

## Scope

- Once the test has made its assertions, remove the preflight root it
  provoked: the parent of `CleanupHookState->Clone`, including `clone.held`
  in the `substitute` mode. The test caused that directory to be kept, so it
  owns it. Keep every existing assertion.
- Do not change the product's refusal, and do not add a test-only seam to
  move the preflight directory.
- Check the other rows in `tests/sqlite_contract` for the same pattern. None
  is expected, because no other product code calls `temp_directory_path`.

## Acceptance

- AC1 On Linux, with `TMPDIR` pointed at an empty directory, a run of
  `sqlite_review_ledger_storage` passes and leaves no
  `bpr-ledger-preflight-*` entry. The row still asserts the refusal and the
  surviving sentinel.
- AC2 A negative control proves the check can fail. With the new removal
  taken out, the same run leaves the two directories again.
- AC3 On macOS and Windows the row passes and leaves no
  `bpr-ledger-preflight-*` entry in the temporary directory. The code is
  portable, so this is expected there too, but it has not been measured.
