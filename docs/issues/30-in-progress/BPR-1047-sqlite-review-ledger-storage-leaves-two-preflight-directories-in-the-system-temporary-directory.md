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

## The fix

`test_preflight_cleanup_preserves_substitutes_and_nonempty_directories` now
calls `remove_retained_preflight_root(CleanupHookState->Clone)` in each mode.
The call comes after all of the mode's existing assertions: the explicit
`unavailable` refusal, the surviving sentinel and the absent private canary.
The helper first checks that the directory is the product's own. The hook's
path must end in `clone`, its parent must be named
`bpr-ledger-preflight-…`, and that parent must be exactly
`temp_directory_path() / <that name>`, which is how
`make_preflight_directory` builds it. The helper then removes that root,
including `clone.held` in the `substitute` mode, and requires that nothing
of it remains. A failed removal names the operating system's error. The
product's refusal is unchanged, and no seam was added.

The other rows in `tests/sqlite_contract` do not show the pattern. All 19
rows registered on Linux ran once in one private `TMPDIR` and left it empty
(see the table below). `set_review_ledger_preflight_cleanup_hook_for_testing`
has no other caller.

## Linux run (bpr-main-linux, 2026-09-23)

WSL2 Ubuntu, GCC 15.2, `build/dev` (Debug, `-Werror`). Every run went
through `ctest -j1`, offscreen, with fresh `HOME` and XDG roots and `TMPDIR`
set to a new empty owner-private directory. Logs are under
`build/logs/bpr-1047/` on this station and are not committed.

| Check | Result |
| --- | --- |
| `sqlite_review_ledger_storage` at `b60ce0d0`, before the change | passed in 1.06 s and left the two directories: `…/clone/substitute-sentinel` with `…/clone.held/{.identity,ledger.db,ledger.db-wal,ledger.db-shm}`, and `…/clone/foreign-child` |
| AC1: the same row with the change | passed in 1.03 s; `TMPDIR` empty afterwards |
| The same, with `TMPDIR` ending in `/` (as macOS sets it) | passed in 1.03 s; `TMPDIR` empty afterwards |
| AC2, negative control: the call replaced by `static_cast<void>(&remove_retained_preflight_root);` | passed in 1.06 s and left the same two directories again, of the same shapes; the call was then restored |
| All 19 `tests/sqlite_contract` rows registered on Linux, in one private `TMPDIR` | 19/19 passed (`sqlite_epoch_catalog_behavior_contract` 121.75 s); `TMPDIR` empty afterwards |
| `clang-format` 22; clang-tidy (LLVM 21) on the changed unit; `git diff --check` | clean |

The first attempt at the negative control deleted the call outright. The
build then failed on the now-unused helper (`-Werror=unused-function`), so
ctest ran the previous binary, which still had the removal, and that run
proves nothing. The control was repeated with the helper's address taken
instead, and that run is the one recorded above.

## Leftovers removed from the shared `/tmp`

Before removal, a script checked each `/tmp/bpr-ledger-preflight-*`
directory. Every one had to meet all of these:

- named `bpr-ledger-preflight-` plus 32 hex digits;
- a real directory owned by this user, mode `0700`, with no symbolic link
  inside;
- a complete tree that is exactly one of the two shapes this test leaves:
  - `substitute`: `clone/substitute-sentinel` and
    `clone.held/{.identity, ledger.db, ledger.db-wal, ledger.db-shm}`;
  - `nonempty`: `clone/foreign-child`;
- a sentinel holding exactly `must-survive`;
- no file containing the canary `BPR_PREFLIGHT_PRIVATE_CLONE_0250`;
- no process with a descriptor or working directory inside it.

All six present met every check. There was one pair per run, each pair made
within the same second, `substitute` first. All six were removed by exact
name, and none is left:

| Run | `substitute` | `nonempty` |
| --- | --- | --- |
| 09:55:13 | `bpr-ledger-preflight-8f6ea9a7759733f827e6b5470a8c82c2` | `bpr-ledger-preflight-c4f692b57baa3ff085747ca0c9178dcd` |
| 18:57:09 | `bpr-ledger-preflight-c1a0e6b20c8a4d3cb5b6b6ebea2f32c8` | `bpr-ledger-preflight-c28002e42fe86a34918a377387732322` |
| 19:37:39 | `bpr-ledger-preflight-85f6fbc12cb55e3cd55f34108b56df25` | `bpr-ledger-preflight-2401a7b3c1673fd90582b30876ea4e9d` |

The 09:55 pair is the one this card first reported. The 18:57 and 19:37
pairs came from later runs of the row on this station that day, made before
this change.

## Candidate

The change is `84538d65` on main. It touches only the test and this card,
and the Linux runs above were made on its content before the commit.

## Acceptance state

- **AC1** (Linux: the row passes, leaves no `bpr-ledger-preflight-*` entry,
  and still asserts the refusal and the surviving sentinel): **pass.**
- **AC2** (a negative control leaves the two directories again): **pass.**
- **AC3** (macOS and Windows: the row passes and leaves no entry): **owed by
  bpr-main-mac and bpr-main-win.** The code is portable. The directory
  check builds the path the way the product does, so a trailing separator
  on the temporary directory cannot defeat it. Neither platform has run it.

### Still owed: the macOS run (bpr-main-mac)

On a head that contains this card's change, rebuilt:

```sh
t=$(mktemp -d /tmp/bpr1047-XXXXXX)
cmake --build --preset dev --target test_better_pull_review_sqlite_review_ledger_tests
TMPDIR="$t/" ctest --preset dev -j1 -V -R '^sqlite_review_ledger_storage$'
find "$t" -mindepth 1
rmdir "$t"
```

Expect: the row passes, and `find` prints nothing. Record any
`FAILED: the test removes the preflight root…` line with its error text.

### Still owed: the Windows run (bpr-main-win)

On a head that contains this card's change, rebuilt:

```powershell
$t = Join-Path $env:TEMP ("bpr1047-" + [guid]::NewGuid())
New-Item -ItemType Directory $t | Out-Null
cmake --build --preset windows-dev --target test_better_pull_review_sqlite_review_ledger_tests
$oldTmp, $oldTemp = $env:TMP, $env:TEMP
$env:TMP = $t; $env:TEMP = $t
ctest --preset windows-dev -j1 -V -R "^sqlite_review_ledger_storage$"
$env:TMP, $env:TEMP = $oldTmp, $oldTemp
Get-ChildItem -Force -Recurse $t
Remove-Item $t
```

Expect: the row passes, and `Get-ChildItem` lists nothing. The row is
uncontained, so it reads `TMP` from this shell. A handle the product still
held would make the removal fail with `FAILED: the test removes the
preflight root it caused the product to keep: <error>`. Record that as a
finding, not as a pass.

When both runs are recorded, the card can finish.

## Windows: the row passes and leaves nothing (bpr-main-win, 2026-09-23)

The run used `b6a31cff`, which contains `84538d65`. `build/windows-dev`
(Visual Studio generator, MSVC 19.51, Debug) rebuilt
`test_better_pull_review_sqlite_review_ledger_tests` at that head. From a
`.cmd`, `TMP` and `TEMP` were pointed at a fresh empty directory,
`C:\Users\mikem\bpr-main-helpers\tmp-1047-a`, and then
`ctest --preset windows-dev -j1 -V -R "^sqlite_review_ledger_storage$"` ran.
The logs are station records under `build/logs/bpr-1047-main-win/` and are
not committed.

- `sqlite_review_ledger_storage` **passed** in 2.57 s
  (`review-ledger-storage-tests:pass`). There was no
  `FAILED: the test removes the preflight root…` line, so no product handle
  was still held.
- **The directory was empty afterwards.** A recursive listing printed
  nothing, and the directory was then removed.
- **Before and after, on the same station.** The user `%TEMP%` holds 77
  `bpr-ledger-preflight-*` directories from earlier runs, in pairs, one pair
  per run. Today's pairs are at 17:59, 19:51 and 20:49. The 20:49 pair came
  from BPR-0887's SQLite selector at `8393e2ed`, before this card's change.
  No pair is dated to this run. The old ones are left in place; they are
  litter from runs before the fix, not this run's.

**AC3 on Windows: pass.** The macOS run is the one still owed.
