---
schema: "implement-rfc.card.v1"
id: "BPR-0885"
title: "The diagnostics log is unconditionally unavailable on Windows, and its root walk creates shared ancestors"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win", "mac"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable logging partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005","largely_fixed_by":"19a3dfd7","linux_done_at":"0e07334b","remaining":"Windows and macOS runs of the logging selector at 0e07334b or later","related_cards":["BPR-0874","BPR-0891","BPR-1030"]}
---

# BPR-0885: The diagnostics log is unconditionally unavailable on Windows, and its root walk creates shared ancestors

## Defect

Two faults in `src/adapters/logging/src/quill_log.cpp`, filed together because
they are the same function's ends.

**1. Windows has no diagnostics log at all.** `open_log` exists only under
`#if !defined(_WIN32)` (`:308-312`). On Windows `File` stays null and
`status()` reports `unavailable` forever. No card in
`docs/issues/{00..50}` names this, so it is not a recorded limitation — it is
just absent. Given how much of this run's Windows work turned on reading what
the application reported, a platform with no log is a real handicap.

**2. The root walk creates shared ancestors and refuses symlinked ones.**
`:271-284` `mkdirat(..., 0700)`s **every missing component** of
`<root>/logs` from `/` downwards — including directories above the storage
root, which are not the application's to create — and opens each with
`O_NOFOLLOW`. So a root under a symlinked ancestor, which on macOS includes
`/var` and `/tmp`, yields `unavailable` with no diagnostic at all. The stores
already have the right shape for this: `validate_private_storage_root`.

## Scope

- Give Windows an `open_log` with the same properties the POSIX one enforces:
  owner-private, no reparse point, single link, exclusive lock. If that cannot
  be done, write down why and make `status()` say so rather than reporting a
  bare `unavailable`.
- Validate the root the way the stores do, and create only the `logs`
  component. Creating `/Users`, `/home` or `/var` is not this component's
  business.
- Report the reason when the log cannot be opened. A silent `unavailable` is
  how both of these survived.
- While there: check that whatever Windows writes honours the redaction
  properties `application_logging_tests.cpp:87-88` pins — no access token,
  refresh token, data-root path or repository name. [[BPR-0891]] covers the
  missing Windows registration of that row and should land with this.

## Acceptance

- Windows either has a log with the stated properties, or a recorded reason and
  an honest status.
- A storage root beneath a symlinked ancestor opens its log on macOS.
- The component never creates a directory above the storage root.
- A failure to open names its cause.

## The premise is stale; the Linux half (bpr-main-linux, 2026-09-23)

Station `bpr-main-linux`: WSL2 Ubuntu 26.04, GCC 15.2, Qt 6.11.2,
`build/dev` (Debug). Rows ran serially through `ctest -j1`, offscreen, with
`DISPLAY` and `WAYLAND_DISPLAY` unset and fresh `HOME` and XDG roots. Logs
are under `build/logs/bpr-main-linux-20260923/bpr-0885/` and are not
committed.

**`19a3dfd7` fixed most of this card before it was picked up.** That commit
is RFC-BPR-0014's release-maintenance slice, from 2026-09-20. It changed
both ends of `open_log`:

- **Windows has a log.** `open_log` has a native branch. The root must pass
  `canonical_windows_root` and `validate_ancestor_chain`, which opens every
  existing ancestor and refuses a reparse point. The root itself is opened
  as a private directory on a reliable local volume. Only `logs` is created,
  through `ensure_private_child`. `diagnostics.log` is opened with
  `FILE_SHARE_READ` alone, so there is one writer, and with
  `FILE_FLAG_OPEN_REPARSE_POINT`. `identity_of` then requires a regular file
  that is not a reparse point and has exactly one link, and
  `validate_private_security` requires the owner-private descriptor. These
  are the four properties the scope names. [[BPR-0891]] read what that
  sink writes on Windows at `cbf944ee`: 43 typed records and no token, path
  or name. [[BPR-1030]] relies on its being the only writer.
- **The POSIX walk from `/` is gone.** The root must already exist as a
  directory. It must be owned by this user and closed to group and other.
  Logging creates only `logs` inside it.
- **Every refusal has a fixed cause.** `open_failure()` returns
  `invalid_root`, `invalid_permissions`, `busy` or `io_failure`. None of
  them carries a path or text.
- `quill_logging` gained two checks: "logger never creates unowned
  ancestors or root" and, on POSIX, "existing symlinked ancestor resolves
  without creating shared directories".

At `78c8ac85`, before any change here, `quill_logging` and
`quill_logging_backpressure` pass on Linux. The strengthened test described
below also passes against that unchanged adapter.

**What was left, and the change (`0e07334b`).**

1. The POSIX `open_log` called the throwing `std::filesystem::canonical`
   between its `lstat` and its `open`. A root removed or renamed in that
   window threw. The sink then said `unavailable` and `open_failure()` said
   `none`, which is the silent refusal this card is about. The root is now
   opened by the name it was given, with `O_NOFOLLOW`. The existing
   device/inode check against the `lstat` refuses a root that changed.
   Ancestors still resolve through symlinks, the way the stores' do. No
   code path in the POSIX `open_log` can throw now, apart from allocation.
2. A throw while the sink is being admitted now records `io_failure`. The
   catch-all used to leave `none`. If the file was admitted and the backend
   fails later, `open_failure()` stays `none`, because the file did open.
   That failure shows in `status()`.
3. `quill_logging` now requires every refusal to name its cause. On POSIX
   the exact cause is required. On Windows any cause other than `none` is
   accepted, because that admission chain can name the same refusal
   differently. For example, it reports a missing ancestor, which
`validate_ancestor_chain` refuses, as `invalid_permissions`. The test
   also covers two new POSIX refusals: a root that is itself a link
   (`invalid_root`) and a root its group can write (`invalid_permissions`).
   Neither may leave a `logs` directory behind.

**What the composition does with the cause.** Outside tests, nothing reads
`open_failure()`. `github_application.cpp` builds the log and ignores its
status. That did not change here, for two reasons. The file is pinned
composition code. And the shipped-platform courses
`application_profile_management_public_path` and
`application_profile_multi_window_public_path` already fail with "this
platform reports its diagnostics file sink" if the sink is unavailable. A
platform that loses its log now fails a course; it no longer passes
silently. Naming the cause in a startup line would be a follow-up.

**Negative controls.** Each is a temporary edit, built and run once, and
then restored. The candidate files were compared byte for byte afterwards.

| Temporary change | `quill_logging` |
| --- | --- |
| `open_log` creates the root and its ancestors first (the old walk) | fails: "logger never creates unowned ancestors or root" |
| `open_log` refuses a root whose canonical path differs (no symlinked ancestors) | fails: "existing symlinked ancestor resolves without creating shared directories" |
| the `lstat` refusal returns `none` | fails: "a root that is itself a link is refused, named, and left untouched" |
| the `lstat` refusal throws, the new catch naming is removed, and the check is the Windows one (`!= none`) | fails at the same assertion |
| the same throw with the new catch naming kept | passes: the throw is named `io_failure` |

**Windows compile check.** No MSVC or MinGW is installed on this station.
The test's Windows branch was type-checked by copying the test with that
`#if` forced on and compiling it with the file's own flags
(`-fsyntax-only -Werror`, GCC 15.2). It passed. A trial that misspelled
`failure::none` in that branch failed with "'nonexistent' is not a member
of 'failure'". The adapter change outside the POSIX branch is the portable
catch in `quill_log::implementation`.

**Verification (Linux).** The change was built and measured as the local
commit `68787d73`, on `78c8ac85`. It reached main as `0e07334b`, rebased
onto `dd4377bf`. The logging adapter, its test and the native storage header
are byte-identical between the two. After the rebase, all six logging rows
ran again, the last row in the table.

| Check | Result |
| --- | --- |
| `quill_logging`, `quill_logging_backpressure`, before (`78c8ac85`) | Passed |
| The strengthened `quill_logging` against the unchanged adapter | Passed |
| `quill_logging`, `quill_logging_backpressure`, candidate | Passed, `quill-logging-tests:pass` |
| The five negative controls | As in the table above |
| `application_logging`, `application_profile_management_public_path`, `application_profile_multi_window_public_path`, `application_release_maintenance_public_path` | 4/4 passed (5.64, 3.81, 10.16, 6.51 s) |
| `clang-format` 22 on both files; `git diff --check` | Clean |
| `clang-tidy` 21 (`_coding-standard/scripts/tidy.py`) on both files | Passed |
| `application_composition_contract` (builds the tests-disabled graph, which compiles `quill_log.cpp` in Release) | Passed, 460.40 s |
| After the rebase, at `0e07334b`: the two `quill_logging` rows and the four composed rows | 6/6 passed |

`src/adapters/logging` and `tests/logging` are not pinned composition code,
so no pin moves.

**Acceptance, per criterion.**

- *Windows has a log with the stated properties.* Implemented at
  `19a3dfd7`, and [[BPR-0891]] observed it writing. The run at this
  candidate is still owed; see below.
- *A storage root beneath a symlinked ancestor opens its log on macOS.*
  Passes on Linux through the same POSIX branch. `quill_logging` passed on
  macOS at `19a3dfd7` (RFC-BPR-0014 M4, `docs/verification/RFC-BPR-0014.md`).
  But this change edits the POSIX branch, so macOS owes a run at this
  candidate.
- *The component never creates a directory above the storage root.* Passes
  on Linux, and the first negative control fails it. On Windows only
  `ensure_private_child` creates anything. The portable assertion runs
  there too.
- *A failure to open names its cause.* Passes on Linux, with the exact
  cause, for every refusal the test makes. The third to fifth negative
  controls fail or pass as expected. Windows owes the portable check.

## What Windows and macOS owe

- **Windows (bpr-main-win).** Build `windows-dev` at `0e07334b` or later,
  then run:

      ctest --preset windows-dev --output-on-failure -R "^(quill_logging|quill_logging_backpressure|application_logging|application_profile_management_public_path|application_profile_multi_window_public_path|application_release_maintenance_public_path)$"

  Expected: 6/6 passed, and `quill_logging` prints `quill-logging-tests:pass`.
  A failure that ends "..., and named" means a Windows refusal returned no
  cause, which is this card's fourth criterion. `quill_logging` is
  registered contained (it has no `WINDOWS_UNCONTAINED` pin). If it fails
  before the logger runs, at "test owner creates private root before logger
  admission", that is a containment question like [[BPR-0874]]'s, not this
  card's. Measure it before pinning.
- **macOS.** Build `dev` at `0e07334b` or later, then run:

      ctest --preset dev --output-on-failure -R '^(quill_logging|application_logging|application_profile_management_public_path)$'

  Expected: 3/3 passed, and `quill-logging-tests:pass`. The criterion
  itself is the assertion "existing symlinked ancestor resolves without
  creating shared directories".

## Windows: the logger passes uncontained; the row stops before it contained (bpr-main-win, 2026-09-23)

`build/windows-dev` (Visual Studio generator, MSVC 19.51, Debug, Qt 6.11.2)
was rebuilt at `664d6e36`, which contains `0e07334b`, and the build exited
0. The owed selector then ran serially and verbose. Only five of its six
names are registered on Windows: `quill_logging_backpressure` is inside
`if(UNIX)` in `tests/logging/CMakeLists.txt`.

| Row | Result |
| --- | --- |
| `application_logging` | Passed, 7.22 s. It prints `EPOCH_EFFECT_FAILED:revision-prepare:0` on purpose, as [[BPR-1040]] notes |
| `application_profile_management_public_path` | Passed, 14.00 s |
| `application_profile_multi_window_public_path` | Passed, 35.17 s |
| `application_release_maintenance_public_path` | **Failed**: `FAILED: native full-profile inventory returns`. This is [[BPR-1038]]'s step, where purge refuses the Git isolation's inherited descriptors. It is not BPR-1040's intermittent Git failure, and it is deterministic, so it was not re-run. |
| `quill_logging` | **Failed** contained in 0.05 s: `test owner creates private root before logger admission` |

**`quill_logging`, measured as the card asks, before any pin.** The same
`quill_logging.exe` from the `windows-dev` build was run directly, twice:

- **Uncontained**, as its own process: exit 0, `quill-logging-tests:pass`.
  That covers every assertion, including "existing symlinked ancestor
  resolves without creating shared directories" and the refusal-cause
  checks. No "..., and named" failure appeared.
- **Contained**, through `better_pull_review_network_sandbox.exe` with the
  registered command: exit 1 at `test owner creates private root before
  logger admission`.

The failing step is the test's own setup, before the logger runs:
`private_root()` calls `make_storage_root_owner_private` on a
`QTemporaryDir` under the container's temporary directory. That is the
helper the `sqlite_*_storage` rows are pinned for as `dos-volume-name` in
`scripts/verify_test_network_isolation.py`. So this is the containment
question the card anticipated, like [[BPR-0874]]'s, and not this card's
logger. Whether this exact call refuses at `GetFinalPathNameByHandleW` was
not traced. The measurement shows only that the setup step fails inside
the container and the whole test passes outside it. The fitting
registration change is a `WINDOWS_UNCONTAINED` pin with `dos-volume-name`,
as the storage rows have. It is not made here: this record is the card path
only, and the pin is the card owner's decision.

For this card, then, Windows shows the log working and every assertion
passing uncontained. Two rows in the selector remain red for other cards'
reasons: `quill_logging` contained, for the containment pin, and release
maintenance, for [[BPR-1038]]. The card stays in Ready for macOS. The logs
are station records under `build/logs/bpr-0885-main-win/` and are not
committed.

## The macOS half (bpr-main-mac, Mac batch 5, 2026-09-23)

Run on `073c7b09`, which contains `0e07334b`. Apple Silicon, AppleClang,
Qt 6.11.2, `build/dev` Debug, reconfigured and rebuilt first (exit 0, no
`error` or `warning:` line). The owed selector ran once, verbose:
`ctest --preset dev --output-on-failure -V -R '^(quill_logging|application_logging|application_profile_management_public_path)$'`.

| Row | Result |
| --- | --- |
| `application_logging` | Passed, 6.92 s |
| `application_profile_management_public_path` | Passed, 3.85 s |
| `quill_logging` | Passed, 0.33 s (`quill-logging-tests:pass`) |

3/3, ctest exit 0. The one-minute load was 10.1 at the start.

**The criterion held on macOS.** "existing symlinked ancestor resolves
without creating shared directories" is an assertion in
`tests/logging/quill_logging_tests.cpp`, inside the `#if !defined(_WIN32)`
block just before the pass line. It prints only if it fails, so
`quill-logging-tests:pass` means a logger rooted under a symlinked parent
opened with `status::available` and `failure::none`. With the Windows
record above, both platforms have run the owed selector. What remains on
Windows is the containment pin, which is the card owner's decision.
