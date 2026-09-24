---
schema: "implement-rfc.card.v1"
id: "BPR-1047"
title: "sqlite_review_ledger_storage leaves two bpr-ledger-preflight-* directories in the system temporary directory after every run"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-main-linux, BPR-0883's runs","confirmed_at":"78c8ac85","introduced_by":"38c10c33 (BPR-0250)","related_cards":["BPR-0250","BPR-0883"],"run":"bpr-main-linux-20260923","reservation":"docs/issues/_reservations/BPR-1046--BPR-1047--4cc848046b0e.json"}
acceptance: [{"criterion":"AC1 On Linux, with TMPDIR pointed at an empty directory, sqlite_review_ledger_storage passes and leaves no bpr-ledger-preflight-* entry, and still asserts the refusal and the surviving sentinel","outcome":"pass","revision":"84538d65","evidence":{"kind":"command","detail":"bpr-main-linux, build/dev, ctest -j1 with a fresh owner-private TMPDIR: passed in 1.03 s and TMPDIR was empty, also with a trailing '/'; all 19 tests/sqlite_contract rows in one TMPDIR left it empty.","path":"docs/issues/60-done/BPR-1047-sqlite-review-ledger-storage-leaves-two-preflight-directories-in-the-system-temporary-directory.md#linux-run-bpr-main-linux-2026-09-23"}},{"criterion":"AC2 A negative control proves the check can fail: with the new removal taken out, the same run leaves the two directories again","outcome":"pass","revision":"84538d65","evidence":{"kind":"command","detail":"Linux: with the call replaced by static_cast<void>(&remove_retained_preflight_root) the row passed and left both directories in their original shapes. macOS repeated the control through the direct sandboxed run and saw the same two directories.","path":"docs/issues/60-done/BPR-1047-sqlite-review-ledger-storage-leaves-two-preflight-directories-in-the-system-temporary-directory.md#linux-run-bpr-main-linux-2026-09-23"}},{"criterion":"AC3 On macOS and Windows the row passes and leaves no bpr-ledger-preflight-* entry in the temporary directory","outcome":"pass","revision":"8e49aae7 (macOS); b6a31cff (Windows)","evidence":{"kind":"command","detail":"macOS (bpr-main-mac): the direct sandbox-exec run of the row's executable with a kept TMPDIR passed and left it empty; its negative control left both directories, so the route can fail (the ctest route cannot on macOS and is not counted). Windows (bpr-main-win): uncontained row, TMP/TEMP pointed at a fresh directory, ctest passed in 2.57 s and the directory was empty afterwards.","path":"docs/issues/60-done/BPR-1047-sqlite-review-ledger-storage-leaves-two-preflight-directories-in-the-system-temporary-directory.md#closed-ac3-is-met-on-macos-and-windows-bpr-main-linux-2026-09-24"}}]
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
- **AC3** (macOS and Windows: the row passes and leaves no entry):
  **macOS: pass** at `8e49aae7`, measured by a direct sandboxed run of the
  row's executable, because the ctest route cannot fail on macOS (see
  [macOS run](#macos-run-bpr-main-mac-2026-09-23)). **Windows: pass** at
  `b6a31cff` (see
  [Windows](#windows-the-row-passes-and-leaves-nothing-bpr-main-win-2026-09-23)).
  The code is portable. The directory check builds the path the way the
  product does, so a trailing separator on the temporary directory cannot
  defeat it.

### The macOS run as first proposed (bpr-main-mac)

This command was run on 2026-09-23 and passed, but on macOS it cannot
fail. The [macOS run](#macos-run-bpr-main-mac-2026-09-23) below records why,
and the check used instead.

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

## macOS run (bpr-main-mac, 2026-09-23)

This is the run the Windows section above names as still owed. macOS 26.6.2
on Apple Silicon, Apple clang 21.0.0, `build/dev` (Debug,
Ninja). The product, test and CMake sources were those of `8e49aae7`, which
contains `84538d65`. `cmake --build --preset dev --target
test_better_pull_review_sqlite_review_ledger_tests` exited 0 with no
`warning` or `error:` line, and it recompiled `review_ledger_storage_tests.cpp`.
Logs are under `build/logs/bpr-1047-mac/` on this station and are not
committed.

### On macOS the ctest route cannot fail

On macOS every row that `bpr_add_network_isolated_test` registers runs
through `scripts/run_network_isolated_test.py`. The runner makes
`/tmp/bpr-net-<random>/fixture`, sets the child's `TMPDIR` to it, and removes
the whole `bpr-net-*` tree through `tempfile.TemporaryDirectory` when the row
ends. So the `TMPDIR` handed to ctest never reaches the test, and whatever
the test leaves is deleted with the runner's root. The negative control
below shows it: with the removal taken out, the card's command still passed
and `$t` stayed empty.

This does not affect the other two platforms. On Linux the before-change run
left the two directories in the `TMPDIR` it was given, so the runs above
measure what they claim. On Windows the row is `WINDOWS_UNCONTAINED` and runs
with no runner, so it reads `TMP` from the shell, as the Windows command
expects.

### The check used instead

The row's executable ran directly under the same sandbox profile, from the
row's working directory, with the row's `BPR_AUTOMATED_TEST_BOUNDARY=1` and
with `TMPDIR` set to a fresh directory. The shell set none of the variables
the row unsets. The runner's derived profile only adds allowances to the
template, for its fixture directory and its loopback ports (none for this
row), so the template alone confines the test at least as tightly.

```sh
repo=/Users/jaeger/Documents/GitHub/BetterPullReview
t=$(mktemp -d /tmp/bpr1047-direct-XXXXXX)
cd "$repo/build/dev/tests/sqlite_contract"
BPR_AUTOMATED_TEST_BOUNDARY=1 TMPDIR="$t/" /usr/bin/sandbox-exec \
    -f "$repo/cmake/sandbox/macos-network-deny.sb" \
    "$repo/build/dev/tests/sqlite_contract/better_pull_review_sqlite_review_ledger_tests"
find "$t" -mindepth 1
rmdir "$t"
```

| Check | Result |
| --- | --- |
| The card's command, at `8e49aae7` | passed in 0.94 s; `find` printed nothing; `rmdir` succeeded |
| The direct run, at `8e49aae7` | `review-ledger-storage-tests:pass`, exit 0, 0.58 s; `$t` empty afterwards |
| Negative control, the card's command | passed in 0.95 s; `find` printed nothing, so this route cannot fail |
| Negative control, the direct run | `review-ledger-storage-tests:pass`, exit 0; `$t` held the two directories (below) |
| Restored and rebuilt, the card's command | passed in 0.87 s; `find` printed nothing |
| Restored and rebuilt, the direct run, `TMPDIR` with and without a trailing `/` | both `review-ledger-storage-tests:pass`, exit 0, no `FAILED` line; `$t` empty afterwards |

The negative control was the Linux one: the call replaced by
`static_cast<void>(&remove_retained_preflight_root);`, and only the test
target rebuilt (exit 0). The direct run then left two directories in `$t`,
both mode `0700`, of the same shapes as on Linux:

- `bpr-ledger-preflight-71b1…` (`substitute`): `clone/substitute-sentinel`,
  holding `must-survive`, and `clone.held/` with `.identity`, `ledger.db`
  (0 bytes), `ledger.db-wal` (0 bytes) and `ledger.db-shm` (32 KiB);
- `bpr-ledger-preflight-7198…` (`nonempty`): `clone/foreign-child`.

No file in either held the canary `BPR_PREFLIGHT_PRIVATE_CLONE_0250`. That
`$t` was removed by exact path. The test source was then restored with
`git checkout`, so its blob is again `65662606`, and the target was rebuilt
before the last two rows of the table.

Before these runs, the top level of `/tmp` and of the per-user temporary
directory (`getconf DARWIN_USER_TEMP_DIR`) held no `bpr-ledger-preflight-*`
entry. Neither did the eight stale `/tmp/bpr-net-*` runner roots from
earlier days, which this card leaves alone. This station has no leftovers
to remove.

## Closed: AC3 is met on macOS and Windows (bpr-main-linux, 2026-09-24)

Both runs were checked against AC3 before this card finished. AC3 asks that
the row passes and leaves no `bpr-ledger-preflight-*` entry in the temporary
directory.

- **macOS: pass, and the check could fail.** A macOS ctest run cannot show a
  leftover: the runner hands the test its own `/tmp/bpr-net-*/fixture` and
  deletes it when the row ends. The pass therefore rests on the direct
  sandboxed run of the row's executable at `8e49aae7`, which contains
  `84538d65`. It used the same sandbox profile and a `TMPDIR` that was kept
  and read afterwards. The row passed and `TMPDIR` was empty, with and
  without a trailing `/`. The same direct run with the removal taken out
  left both directories, in the Linux shapes. So this route sees a leftover
  when there is one. The ctest result is not counted.
- **Windows: pass.** At `b6a31cff`, which contains `84538d65`,
  `sqlite_review_ledger_storage` passed with `TMP` and `TEMP` pointed at a
  fresh empty directory. The directory was empty afterwards.
  - The row is in `WINDOWS_UNCONTAINED_TESTS`
    (`scripts/verify_test_network_isolation.py`, `dos-volume-name`), so it
    runs with no runner and reads `TMP` from the shell.
  - The test's removal accepts only
    `temp_directory_path() / bpr-ledger-preflight-…`, the path the product
    builds in the same process, and it fails the row if anything of that
    root remains. So a pass means the root was removed from the directory
    the product used.
  - There was no Windows negative control. The Linux and macOS controls show
    that the check fails when the removal is taken out, and the change has
    no platform branch.

Neither the test nor `review_ledger_database.cpp` has changed since
`84538d65`.

| Criterion | Result |
| --- | --- |
| AC1 Linux: passes, leaves no entry, still asserts the refusal and the sentinel | **Pass** on `84538d65`'s content |
| AC2 A negative control leaves the two directories again | **Pass** on Linux, and repeated on macOS through the direct run |
| AC3 macOS and Windows: passes and leaves no entry | **Pass**: macOS at `8e49aae7` by the direct run, Windows at `b6a31cff` |

The card finishes.
