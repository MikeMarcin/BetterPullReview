---
schema: "implement-rfc.card.v1"
id: "BPR-1118"
title: "sqlite_profile_activation leaks /tmp/bpr-act-* and /tmp/bpr-xdg-* when an expectation fails, because expect calls std::exit"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["linux"]
metadata: {"found_by":"BPR-1113's negative controls, which removed the leftovers by name after each run (d03e2d61)","related_cards":["BPR-1113","BPR-0906","BPR-0881","BPR-0979"],"reservation":"docs/issues/_reservations/BPR-1117--BPR-1118--7bb2c9ebce22.json"}
---

# BPR-1118: sqlite_profile_activation leaks /tmp/bpr-act-* and /tmp/bpr-xdg-* when an expectation fails, because expect calls std::exit

## Why

`tests/sqlite_contract/profile_activation_tests.cpp` makes its scratch roots
with `mkdtemp`:

- each `scratch_directory` is a `/tmp/bpr-act-XXXXXX` root. It is grown to a
  real profile root's length with two nested directories;
- the Linux runtime-directory courses make `/tmp/bpr-xdg-XXXXXX`.

Each is removed by a destructor or by a call at the end of its course. But
`expect` reports a failed expectation with `std::exit(EXIT_FAILURE)`, which
runs no destructor of a local object. So every failing run leaves its roots in
`/tmp`. So does an exception the test does not catch, because it ends in
`std::terminate`.

[[BPR-1113]]'s negative controls hit this. They removed each run's
leftovers by exact name. Two older leftovers from 2026-09-23 are still in
`/tmp`: `/tmp/bpr-act-mWIPEe` and `/tmp/bpr-act-Kwl7Qp`.

## Scope

- A failed expectation, or an exception, still fails the row with its
  `FAILED:` line. It then unwinds, so every scratch root is removed.
- Each root is owned from the moment it exists. That includes a
  `scratch_directory` whose constructor fails after `mkdtemp`, and the
  `bpr-xdg` directories.
- A course that rewrites `XDG_RUNTIME_DIR` restores it before its scratch
  root goes. The root's endpoint is then removed where it resolved in the
  inherited environment, as on the passing path.
- Record what still leaves a root: a kill, a CTest timeout, an abort or a
  crash.
- Prove the two 2026-09-23 leftovers are this test's before removing them by
  exact name, and record them.

## Acceptance

- AC1 An expectation that fails at any stage of the POSIX courses leaves no
  `/tmp/bpr-act-*` or `/tmp/bpr-xdg-*`, and the row still fails with that
  expectation's `FAILED:` line. The stages are:
  - a scratch root half built;
  - a server bound in a private runtime directory;
  - a widened runtime directory;
  - the exact-length runtime directories;
  - a planted symlink;
  - a stale socket.

  A reverted trial at the same stages, with `HEAD`'s `std::exit`, leaves
  them.
- AC2 An exception from the standard library also fails the row with a named
  line and removes the roots. At `HEAD` it aborts and leaves them.
- AC3 The passing row is unchanged on Linux: `sqlite_profile_activation`
  passes and prints the same lines.
- AC4 The two 2026-09-23 leftovers are shown to be this test's, removed by
  exact name, and recorded here.
- AC5 The change is portable, so the row builds and passes on macOS and
  Windows.

## Fixed at `bb1e93f7`, verified on Linux — bpr-main-linux, 2026-09-24

Claimed at `70a1c701`. The change is one commit, `bb1e93f7`, and touches only
`tests/sqlite_contract/profile_activation_tests.cpp`. It was verified before
its rebase. The rebase brought in only documentation, so no `src/`, `tests/`
or `cmake/` path differs.

### What changed

- **A failed expectation unwinds.** `expect` still prints `FAILED: <message>`
  to standard error. It then throws a test-local `expectation_failed` instead
  of calling `std::exit`. `receive_request`'s timeout goes through the same
  `[[noreturn]] fail`, so its `std::abort` is gone.
- **`main` catches.** A failed expectation becomes `EXIT_FAILURE`. So does any
  other exception, which prints `FAILED: unexpected exception: <what>`. Before,
  such an exception ended in `std::terminate`, an abort, with no unwinding.
- **Each root is owned from the moment it exists.** `temporary_tree` owns a
  `mkdtemp` result and removes it with everything in it.
  - `scratch_directory` holds one as its first member. A constructor that
    fails while it grows the root to a real profile root's length still
    removes it, because members already built are destroyed.
  - The two `bpr-xdg` courses hold theirs the same way.
- **`XDG_RUNTIME_DIR` is restored by an owner.** A course that rewrites it
  declares `runtime_directory_restore` after its scratch root. While a
  failure unwinds, the variable is restored first. The root then removes its
  endpoint where the endpoint resolves in the inherited environment, as on
  the passing path. It never follows a value a course set part-way through.
- **Windows.** The Windows `scratch_directory` removes the root it created if
  making it owner-private, or resolving its canonical path, then fails. No
  destructor runs for an object whose constructor throws.

### Verification on Linux

WSL2 Ubuntu 26.04, GCC 15.2, Qt 6.11.2, `build/dev` (Debug). Every run went
through `ctest`, offscreen, with `DISPLAY`, `WAYLAND_DISPLAY` and
`DBUS_SESSION_BUS_ADDRESS` unset. `HOME` and the XDG roots were fresh. A
wrapper recorded, by a read-only listing, any `bpr-` name a run added to
`/run/user/1000`, and any `/tmp/bpr-run-*`, `bpr-act-*` or `bpr-xdg-*` it
left. No run added anything to `/run/user/1000`.

**The trials.** A trial copy of the test was made from `HEAD`'s file and
from the candidate's, with the same hooks:

- each prints every root `mkdtemp` makes;
- each fails at the stage `BPR_TRIAL_FAIL_AT` names, through `expect`. The
  `filesystem-error` stage throws a `std::filesystem::filesystem_error`
  instead.

Each copy was built, and the row was run once per stage. A root the run
printed and that still existed afterwards counts as left. "2 of 4" counts
every root the run made, including those that earlier, passing courses had
already removed. `HEAD`'s leftovers
were listed, then removed by the exact names the run printed.

| Stage | Where it fails | `HEAD` (`std::exit`) | Candidate |
| --- | --- | --- | --- |
| `scratch-extend` | a scratch root half grown, after the private runtime directory exists | exit 1, 2 of 2 roots left | exit 1, 0 of 2 left |
| `xdg-private-bound` | a server bound in the private `XDG_RUNTIME_DIR` | exit 1, 2 of 2 left, the socket still in the `bpr-xdg` root | exit 1, 0 of 2 left |
| `xdg-widened` | the runtime directory widened to 0755 | exit 1, 2 of 2 left | exit 1, 0 of 2 left |
| `filesystem-error` | a `filesystem_error` among the exact-length directories | exit -6 (abort), no `FAILED:` line, 2 of 4 left | exit 1, `FAILED: unexpected exception: filesystem error: trial: Input/output error`, 0 of 4 left |
| `xdg-long` | the directory one byte too long, named | exit 1, 2 of 4 left | exit 1, 0 of 4 left |
| `authority-symlink` | a symlink planted at the endpoint | exit 1, 2 of 7 left | exit 1, 0 of 7 left |
| `handoff-pending` | a client process waiting on a live server | exit 1, 2 of 9 left | exit 1, 0 of 9 left |
| `stale-socket` | a stale socket bound at the endpoint | exit 1, 1 of 11 left | exit 1, 0 of 11 left |

Every candidate run failed with its `FAILED:` line. The candidate file was
then restored, compared byte for byte with the saved copy and its diff, and
rebuilt.

| Claim | Check | Result |
| --- | --- | --- |
| AC1 and AC2 | the trials above | `HEAD` leaves roots at every stage, and aborts on the exception. The candidate leaves none, and names every failure |
| AC3, the passing row | `sqlite_profile_activation` at the candidate | Pass, 4.13 s. It prints the same five lines: `xdg-private=followed`, `xdg-72-bytes=followed:xdg-73-bytes=passed-over`, `xdg-other-owner=passed-over:/root`, the geometry line and `profile-activation-tests:pass` |
| Stable | `ctest --repeat until-fail:5` on `sqlite_profile_activation`, with `msvc_secure_crt_contract` | Both pass. The `std::getenv` of `BPR_TEST_SOCKET_ROOT` keeps its MSVC suppression in its new function |
| Gates | `/usr/bin/clang-format` 22, `--dry-run -Werror`; scoped `tidy.py --warnings-as-errors` on the file; `git diff --check` | Pass |

The logs are station records under `build/logs/bpr-1118/` and are not
committed: `trial-head-*.log`, `trial-candidate-*.log`, both
`trial-*-summary.txt`, `candidate-row.log`,
`candidate-stability-and-crt.log`, `tidy-candidate.log` and
`leftovers-2026-09-23.txt`.

### The two leftovers from 2026-09-23 (AC4)

`/tmp/bpr-act-mWIPEe` and `/tmp/bpr-act-Kwl7Qp` were this test's. The
evidence:

- **The name.** Nothing else in the tree makes a `bpr-act-XXXXXX` name. The
  Windows scratch root is `bpr-act-<clock>-<counter>`.
- **The shape.** Each held exactly two nested directories, named with 48 and
  25 `d`s, and nothing else. That is the growth `scratch_directory` makes
  from a 19-byte `/tmp/bpr-act-XXXXXX` to the 94-byte profile root it asks
  for. Each was mode 0700 and owned by this user.
- **The time.** Their times match, to the second, the starts of two
  [[BPR-0906]] trials at `073c7b09`, each of which failed this row through
  `expect`:
  - `/tmp/bpr-act-mWIPEe`, 18:51:09 -07:00, is
    `build/logs/bpr-0906/trial-T1-no-xdg-candidate.log`. That run started at
    18:51:09 and failed at `a private XDG_RUNTIME_DIR holds the endpoint`.
  - `/tmp/bpr-act-Kwl7Qp`, 18:52:37 -07:00, is `trial-T5-xdg-unverified.log`.
    That run started at 18:52:37 and failed at `a shared XDG_RUNTIME_DIR is
    passed over`.

Both were removed by exact name on 2026-09-24 at 15:17:54 -07:00. The record
is `build/logs/bpr-1118/leftovers-2026-09-23.txt`. No `/tmp/bpr-act-*` or
`/tmp/bpr-xdg-*` is left on this station.

### What still leaves a root

No code runs in the process in these cases:

- a kill by SIGKILL, which is what a CTest timeout sends to the launcher and
  its child;
- an abort;
- a crash.

A root left that way holds only this test's directories, and is removed by
name or when the host clears `/tmp`. As in [[BPR-1113]], a sweep of stale
roots is not added, because it could remove a live run's root.

### Seen elsewhere, not changed here

A text scan finds 30 other test sources that end a failed expectation with
`std::exit(EXIT_FAILURE)` and also make temporary directories. Among them are
most of `tests/sqlite_contract/`, `tests/git_contract/`'s epoch and haves
courses, and `tests/adapters_qt/agent_launch_canary_tests.cpp`. The scan is a
heuristic: it shows the pattern, not that each one leaks. That wants its own
card.

### Evidence owed elsewhere (AC5)

The change is portable:

- the shared `expect`, `fail` and `main` compile on every platform;
- the POSIX `scratch_directory` and `temporary_tree` compile on macOS;
- the Windows `scratch_directory` gained its constructor guard.

This station builds none of the macOS or Windows branches. These runs are
owed from those stations:

```sh
cmake --build --preset dev --target test_profile_activation
ctest --preset dev --output-on-failure -R '^(sqlite_profile_activation|msvc_secure_crt_contract)$'
```

### Where the criteria stand

- **AC1, AC2: pass** on Linux at `bb1e93f7`, by the trials above.
- **AC3: pass.**
- **AC4: pass.** Both leftovers were shown to be this test's, removed and
  recorded.
- **AC5: pending.** The macOS and Windows runs above are owed. The card stays
  In Progress on this station's claim until they are supplied.

## macOS station, 2026-09-24 (bpr-main-mac): the owed macOS run passes

Run at `9e36f157`, which contains `bb1e93f7`, with a clean tree, on Apple
clang 16, arm64, macOS 26.6.2.

- `cmake --build --preset dev --target test_profile_activation` exits 0. It
  rebuilds `profile_activation_tests.cpp` and links it, with no warning.
- `ctest --preset dev --output-on-failure -R
  '^(sqlite_profile_activation|msvc_secure_crt_contract)$'`: 2 of 2 pass.
  `msvc_secure_crt_contract` takes 0.44 s and `sqlite_profile_activation`
  4.57 s.
- No `bpr-act-*` or `bpr-xdg-*` root is left in `/tmp` or `$TMPDIR` after the
  run. There were none before it.

**AC5, macOS half: pass.** The Windows run is still owed. This station leaves
the card where it is, In Progress on the Linux station's claim.

## Windows station, 2026-09-24 (bpr-main-win): the owed Windows run passes

Run at `9ee4ee50`, which contains `bb1e93f7`, and whose
`profile_activation_tests.cpp` is `bb1e93f7`'s. The tree was clean. The
station is Windows 11 10.0.26200, with Visual Studio 18 (MSVC 19.51) and Qt
6.11.2. The tree is `build/windows-dev` (the Visual Studio generator, Debug).
This station ran it at the coordinator's request.

- `cmake --build --preset windows-dev --target test_profile_activation`
  exits 0. It compiles `profile_activation_tests.cpp` and links
  `profile_activation.exe` with no compiler warning. The build re-ran
  configure first, and the only warnings in its log are CMake's
  uninitialized-variable notes from Qt's own package scripts.
- `ctest --preset windows-dev --output-on-failure -R
  "^(sqlite_profile_activation|msvc_secure_crt_contract)$"`: 2 of 2 pass.
  `msvc_secure_crt_contract` takes 0.73 s and `sqlite_profile_activation`
  5.86 s.
- `%TEMP%` held no `bpr-act-*` directory before the run, and holds none
  after it.

The logs are station records under `build/logs/bpr-1118-main-win/` and are
not committed.

**AC5, Windows half: pass.** With the macOS run above, AC5 has its runs
from both stations, and every criterion is met.
