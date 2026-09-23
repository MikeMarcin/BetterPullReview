---
schema: "implement-rfc.card.v1"
id: "BPR-1026"
title: "msvc_secure_crt_contract: std::getenv in release_update_* end-to-end tests"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-linux core-only preset run under BPR-0975","red_since":"d1e0f46c","reproduced_at":["a8e9b714","89c6e0f4"],"related_cards":["BPR-0519","BPR-0854","BPR-0948","BPR-0983","BPR-1020"],"reservation":"docs/issues/_reservations/BPR-1024--BPR-1026--f65f9f9f0854.json"}
acceptance: [{"criterion":"AC1 msvc_secure_crt_contract passes 3 of 3 on macOS and Linux at the fix, and on Windows, where it is registered too","outcome":"pass","revision":"f9c3161d","evidence":{"kind":"command","detail":"Windows (bpr-main-win, build/dev MSVC 19.51 Debug at f9c3161d, contains bac4c745): ctest --test-dir build/dev -j1 -V -R '^(msvc_secure_crt_contract|application_release_update_public_path)$': msvc_secure_crt_contract Passed 0.65 s, 'Ran 3 tests', 'OK'. Linux at e648db24: pass 0.36 s, Ran 3 tests OK. macOS at bac4c745: pass 0.30 s (BPR-0854's Mac record).","path":"docs/issues/60-done/BPR-1026-msvc-secure-crt-contract-flags-getenv-in-the-release-update-end-to-end-tests.md#the-windows-run-bpr-main-win-2026-09-23"}},{"criterion":"AC2 The contract still fails on an unspelled std::getenv in a Windows-built file (negative control, not committed); every allowlist entry names a file whose CMake registration excludes Windows","outcome":"pass","revision":"e648db24","evidence":{"kind":"command","detail":"bpr-main-linux, git archive export under /tmp: unmodified OK; plant A (unspelled read at file scope in release_update_public_path_tests.cpp, Windows-built) FAILED at :6; plant B (scoped disable : 4996 removed from the transaction helper) FAILED at :269; plant C (read appended to src/core/src/agent_chat_scope.cpp) FAILED at :352; restored OK. NOT_BUILT_ON_WINDOWS names only macos_keychain_native_tests.cpp (if(APPLE)) and linux_secret_service_native_tests.cpp (Linux-only).","path":"docs/issues/60-done/BPR-1026-msvc-secure-crt-contract-flags-getenv-in-the-release-update-end-to-end-tests.md#linux-and-macos-done-at-the-fix-windows-still-owed-bpr-main-linux-2026-09-23"}},{"criterion":"AC3 (as amended under the owner's stale-card leave) Each changed test source builds wherever it is registered and its row gives the same result; on Windows the row builds under MSVC at bac4c745 or later and still passes","outcome":"pass","revision":"f9c3161d","evidence":{"kind":"command","detail":"Windows at f9c3161d: the object for release_update_public_path_tests.cpp deleted and recompiled with /W4 /WX via cmake --build build/dev --target test_release_update_public_path -- -v; no warning printed, exit 0; application_release_update_public_path Passed 7.24 s (release-update-public-path:pass), as at 4fd95fe5. macOS at bac4c745: release_update_transaction pass 0.44 s, application_release_update_public_path pass 4.50 s. Linux at e648db24: pass 3.67 s; the translation unit preprocesses byte-identically before and after the change.","path":"docs/issues/60-done/BPR-1026-msvc-secure-crt-contract-flags-getenv-in-the-release-update-end-to-end-tests.md#the-windows-run-bpr-main-win-2026-09-23"}}]
---

# BPR-1026: msvc_secure_crt_contract: std::getenv in release_update_* end-to-end tests

## Defect

`msvc_secure_crt_contract` (`tests/scripts/msvc_secure_crt_tests.py`,
[[BPR-0519]]) fails on main at `a8e9b714` and `89c6e0f4`, with one failure in its three
tests. `test_every_getenv_read_is_spelled_for_msvc` reports four
`std::getenv("QUALIFICATION_TARGET_ROOT")` reads:

- `tests/end_to_end/release_update_public_path_tests.cpp:301` (`native_state`)
  and `:372` (`native_handoff`);
- `tests/end_to_end/release_update_transaction_tests.cpp:262`
  (`native_signed_recovery`) and `:303` (`native_prepare`).

The row is registered on every platform (`tests/CMakeLists.txt:561`), and it
is in `core-only` too. It is therefore red in every preset, and a new real
finding in a Windows-built file would go unnoticed in a row that already
fails.

**No Windows build break.** MSVC compiles none of the four reads:

- `release_update_transaction_tests.cpp` is built only inside `if(APPLE)`
  (`tests/end_to_end/CMakeLists.txt:2703–2709`).
- `release_update_public_path_tests.cpp` is built everywhere, but both of its
  reads sit inside `#if defined(__APPLE__)` (lines 296–435). The Windows
  station built and ran that test at `38cc26d7` for [[BPR-1020]], where it
  failed only at a late course assertion.

The contract reads text, not branches. Its per-file allowlist,
`NOT_BUILT_ON_WINDOWS`, does not name the Apple-only file.

**Not fixed at HEAD; [[BPR-0983]] does not bear on it.** The contract's finder
was run on `git archive` exports:

| Revision | Findings |
|---|---|
| `5ca78697` (before `d1e0f46c`) | none |
| `d1e0f46c` ([[BPR-0948]], 2026-09-20) | `release_update_transaction_tests.cpp:242` |
| `b7e2df46` (2026-09-20, native fault courses) | the four, at public path `:299` and `:370` |
| `a44b2e38` (before [[BPR-0983]]'s `d7175dab`) | the four, at today's lines |
| `d7175dab` ([[BPR-0983]]) | unchanged |
| `a8e9b714` | unchanged |
| `89c6e0f4` (HEAD when this card was written) | unchanged |

[[BPR-0983]] gave the updater's `BPR_AUTOMATED_TEST_BOUNDARY` read under
`src/` a single reader. It changed neither test file.

[[BPR-0854]] (In Progress) fixed this row's earlier finding,
`profile_activation_tests.cpp:39`. Its acceptance, "All four rows pass on
macOS and Linux", cannot be met while these four remain. This card leaves
that card unchanged.

## Scope

Clear the four at their cause, following the tree's precedents:

- `release_update_transaction_tests.cpp` is Apple-only in CMake. Name it in
  `NOT_BUILT_ON_WINDOWS` with that reason, as
  `macos_keychain_native_tests.cpp` is named (built under `if(APPLE)`,
  `tests/github_contract/CMakeLists.txt:902`), or spell the reads.
- `release_update_public_path_tests.cpp` is built on Windows, but its reads
  are Apple-only. [[BPR-0854]] item 2 gave a POSIX-only read the scoped C4996
  suppression and a comment explaining it. Do the same, or read the variable
  with Qt's `qEnvironmentVariable`, as
  `release_maintenance_public_path_tests.cpp:97` does for the same variable.
- Do not weaken the contract. Teach it preprocessor guards only if it stays
  strict for code that Windows compiles.

## Acceptance

- AC1 `msvc_secure_crt_contract` passes 3 of 3 on macOS and Linux at the
  fix, and on Windows, where it is registered too.
- AC2 The contract still fails on an unspelled `std::getenv` in a
  Windows-built file (a negative control, not committed). Every allowlist
  entry names a file whose CMake registration excludes Windows.
- AC3 A changed test source still builds wherever it is registered, and its
  row gives the same result as before the change. For
  `release_update_transaction` that is macOS. For
  `application_release_update_public_path` it is macOS and Linux, plus a
  Windows build, where the row's known late failure ([[BPR-1020]]) is
  unchanged.

## Reproducing

```sh
python3 tests/scripts/msvc_secure_crt_tests.py
```

At `a8e9b714` and `89c6e0f4` on bpr-main-linux: `Ran 3 tests`,
`FAILED (failures=1)`, and the four findings above.

## Superseded at `bac4c745` (bpr-main-linux, 2026-09-23)

[[BPR-0854]] fixed these four reads at `bac4c745`. Each file now reads
`QUALIFICATION_TARGET_ROOT` once, through a `qualification_target_root()`
helper that carries the scoped C4996 suppression
(`release_update_public_path_tests.cpp:305`,
`release_update_transaction_tests.cpp:264`). On bpr-main-linux at `98e5b796`,
`msvc_secure_crt_contract` passes: `ctest` in 0.39 s, and a direct run gives
`Ran 3 tests`, `OK` (`build/r2-logs/bpr0866-bpr0854-rows-98e5b796.log`). The
row is no longer red on Linux.

AC1's Windows run is the one [[BPR-0854]] already asks of bpr-main-win. AC2's
negative control and AC3's builds have not been run against `bac4c745`'s
change. This note does not move the card.

## Linux and macOS done at the fix; Windows still owed (bpr-main-linux, 2026-09-23)

**Decision: the card stays open, and only Windows remains.** The fix is
[[BPR-0854]]'s `bac4c745`. AC1 and AC3 each require a Windows observation,
and no Windows run at or after `bac4c745` is recorded: the last one,
[[BPR-1031]]'s at `4fd95fe5`, predates it. The board has no "superseded"
state, and Done needs a passing record for every criterion, so this card
cannot close as Done or as superseded yet. It stays in Ready and narrows to
`platforms: ["win"]`, so that the Windows queue offers it.

**AC1: the contract passes 3 of 3.**

- Linux, `e648db24`: `ctest --test-dir build/dev -R '^msvc_secure_crt_contract$'
  --output-on-failure` passes in 0.36 s. A direct `python3
  tests/scripts/msvc_secure_crt_tests.py -v` gives `Ran 3 tests`, `OK`
  (`build/r2-logs/bpr1026-rows-e648db24.log`).
- macOS, `bac4c745`: pass, 0.30 s, from BPR-0854's Mac record
  (`build/bpr0854-mac3-rows.log` on that station).
- Windows: owed.

**AC2: the negative control fails, and the allowlist is sound. Pass on Linux
at `e648db24`.** The controls ran in a `git archive` export under `/tmp`,
which was removed afterwards, so the checkout was never edited
(`build/r2-logs/bpr1026-ac2-negative-control-e648db24.log`):

| Export | Result |
|---|---|
| Unmodified | `Ran 3 tests`, `OK` |
| A: an unspelled `std::getenv` at file scope in `release_update_public_path_tests.cpp`, outside its `__APPLE__` regions. Windows builds this file. | `FAILED (failures=1)`: `tests/end_to_end/release_update_public_path_tests.cpp:6` |
| B: the scoped `disable : 4996` removed from `release_update_transaction_tests.cpp`'s helper | `FAILED`: `release_update_transaction_tests.cpp:269`. The fix did not allowlist this Apple-only file, so the contract still checks its read. |
| C: an unspelled read appended to `src/core/src/agent_chat_scope.cpp` | `FAILED`: `src/core/src/agent_chat_scope.cpp:352` |
| Restored | `OK` |

`bac4c745` did not change `NOT_BUILT_ON_WINDOWS`. It still names two files,
and CMake registers each only where Windows is excluded:

- `macos_keychain_native_tests.cpp`, inside `if(APPLE)`
  (`tests/github_contract/CMakeLists.txt:902–1004`);
- `linux_secret_service_native_tests.cpp`, inside
  `if(CMAKE_SYSTEM_NAME STREQUAL "Linux")` (`:1006–1099`).

No other CMake file names either one.

**AC3: each changed source still builds, with the same row result.**

- `release_update_transaction` (macOS only): pass at `bac4c745`, 0.44 s, from
  BPR-0854's Mac record. Before the change, it passed in RFC-BPR-0035's M4
  candidate verification (`docs/verification/RFC-BPR-0035.md`, 9/9 on macOS,
  source `518c384a`, which contains both `d1e0f46c` and `b7e2df46`).
- `application_release_update_public_path`:
  - macOS: pass at `bac4c745`, 4.50 s. Before the change, it passed in the
    same M4 run.
  - Linux: pass at `e648db24`, 3.67 s. The change cannot move this result.
    The file was preprocessed with its own Linux compile command from
    `compile_commands.json`, using `-E -P`, at `bac4c745^`, `bac4c745` and
    `e648db24`. All three outputs are byte-identical (SHA-256
    `d6d025298fbf…9a6c93af`), because the helper and both of its call sites
    sit under `#if defined(__APPLE__)`. The file has not changed since
    `bac4c745`.
  - Windows: an MSVC build is owed.

**AC3's Windows expectation is out of date.** This is an amendment, not an
observation. AC3 expects the row's "known late failure ([[BPR-1020]])" to be
unchanged, but [[BPR-1031]] has since fixed that failure. The row passed on
bpr-main-win at `4fd95fe5` (7.33 s, BPR-1031's AC1), a revision that does not
contain `bac4c745`. So the Windows result to match is now a pass. The owner
has given standing leave to update stale cards. Under it, AC3's Windows part
now reads: the row builds under MSVC at `bac4c745` or later, and still
passes. The acceptance text above is left as written, and this note is the
amendment.

### Still owed: one Windows run (bpr-main-win)

Rebuild `build/dev` at `bac4c745` or later, then make one serial run:

```sh
ctest --test-dir build/dev -j1 --output-on-failure -R '^(msvc_secure_crt_contract|application_release_update_public_path)$'
```

Observe:

- `msvc_secure_crt_contract` passes, 3 of 3 (AC1). This same run is the
  `msvc_secure_crt_contract` half of [[BPR-0854]]'s Windows line.
- `test_release_update_public_path` builds under MSVC with no warning, and
  `application_release_update_public_path` passes, as it did at `4fd95fe5`
  (AC3 as amended).

If both hold, the card can finish with the records above. macOS and Linux
owe nothing more.

## The Windows run (bpr-main-win, 2026-09-23)

JaegAW, Windows 11 10.0.26200, `build/dev` (Ninja, MSVC 19.51 Debug,
`/W4 /WX`, Qt 6.11.2), rebuilt at `f9c3161d`, which contains `bac4c745`. The
logs are station records under `build/logs/bpr-0866-1026-main-win/` and are
not committed.

- **The build has no warning.** The object for
  `release_update_public_path_tests.cpp` was deleted, then
  `cmake --build build/dev --target test_release_update_public_path -- -v`
  recompiled it with the tree's `/W4 /WX` and relinked the course. The only
  output is the two command lines and the file name. Nothing else is
  printed, and the build exits 0.
- **The rows pass.** The run was
  `ctest --test-dir build/dev -j1 --output-on-failure -V -R '^(msvc_secure_crt_contract|application_release_update_public_path)$'`:
  - `msvc_secure_crt_contract` passed (0.65 s), with `Ran 3 tests`, `OK` (AC1).
  - `application_release_update_public_path` passed (7.24 s), with
    `release-update-public-path:pass`. That matches its pass at `4fd95fe5`
    (AC3 as amended).

With this run every criterion has a passing record: AC1 on all three
platforms, AC2 on Linux, and AC3 on all three platforms.
