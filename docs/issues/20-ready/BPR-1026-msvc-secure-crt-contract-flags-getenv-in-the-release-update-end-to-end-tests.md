---
schema: "implement-rfc.card.v1"
id: "BPR-1026"
title: "msvc_secure_crt_contract: std::getenv in release_update_* end-to-end tests"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-main-linux core-only preset run under BPR-0975","red_since":"d1e0f46c","reproduced_at":["a8e9b714","89c6e0f4"],"related_cards":["BPR-0519","BPR-0854","BPR-0948","BPR-0983","BPR-1020"],"reservation":"docs/issues/_reservations/BPR-1024--BPR-1026--f65f9f9f0854.json"}
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
