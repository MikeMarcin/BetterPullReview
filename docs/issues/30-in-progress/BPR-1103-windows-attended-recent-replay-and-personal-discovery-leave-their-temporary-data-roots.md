---
schema: "implement-rfc.card.v1"
id: "BPR-1103"
title: "Windows: application_attended_recent_replay and application_personal_discovery_public_path leave their QTemporaryDir data roots in %TEMP% on every run"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1051's Windows measurement and BPR-1050's sweep","found_at":"bcd57840","related_cards":["BPR-1051","BPR-1050","BPR-1047","BPR-1044","BPR-1039"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1102--BPR-1105--5bdc61738290.json"}
---

# BPR-1103: Windows: application_attended_recent_replay and application_personal_discovery_public_path leave their QTemporaryDir data roots in %TEMP% on every run

## Defect

Two rows pass on Windows and leave their temporary directories behind in the
system temporary directory, every run:

| Row | Left per run | Each holds |
| --- | --- | --- |
| `application_attended_recent_replay` | 2 `attended_recent_replay-XXXXXX` | `data/` with `profile-runtime.lock` and five SQLite stores (`appearance-settings`, `epoch-catalog`, `profile-registry`, `release-update`, `review-queue`), in one of the two with their `-wal` and `-shm` files |
| `application_personal_discovery_public_path` | 1 `personal_discovery_public_path-XXXXXX` | `data/` with the same, plus `logs/diagnostics.log`, `.git-process-boundary.lock` and `work/` |

Nothing reports it: the rows pass, and `QTemporaryDir`'s destructor fails
silently. The names are the `QTemporaryDir` default template, the executable's
name and six random characters.

## Measured (bpr-main-win, MSVC 19.51 Debug, Qt 6.11.2)

- [[BPR-1051]]'s Windows runs at `bcd57840`, each row alone with `TEMP` and
  `TMP` pointed at a new empty directory: the replay row left 2 roots
  (30 entries), the discovery row 1 root (24 entries).
- [[BPR-1050]]'s 154-row `^application_` sweep at `bcd57840`: comparing
  `%TEMP%` before and after, these three roots are the only entries any row
  left.
- `%TEMP%` on the station held 21 older ones from 2026-09-23: 14
  `attended_recent_replay-*` and 7 `personal_discovery_public_path-*`. The
  station removed only the six its own runs made; the 21 were left to the
  operator.

## Scope

- Confirm the likely cause, files still open when `QTemporaryDir` removes the
  root.
- Make each course remove its root reliably, and fail loudly if it cannot,
  rather than leak it silently.
- Remove the 21 older leftovers only if they are provably from these courses,
  by their exact name pattern; otherwise list them.

## Acceptance

- AC1 The cause is named with evidence from a real Windows run.
- AC2 Both rows pass on Windows and leave nothing in a fresh `TEMP`, in N of N
  runs (N >= 3 each).
- AC3 Negative control: a course that cannot remove its root fails, naming
  what is left; and the courses as they were leave their roots again.
- AC4 macOS and Linux keep their path, or the change is run there.

## Named, fixed in the courses, and nothing is left (bpr-main-win, 2026-09-24)

Run `bpr-main-win-20260923`, `build/dev` (MSVC 19.51 Debug, Qt 6.11.2),
serial, each run alone with `TEMP` and `TMP` pointed at a new empty directory
that is listed afterwards. At `84c2b3a3` plus this change. The logs are
station records under `build/logs/bpr-1103-main-win/` and are not committed.

**The cause (AC1).** A reverted probe tried an exclusive open of every file
under each course's root, at three moments. `error=32` is
`ERROR_SHARING_VIOLATION`: another handle, the course's own, holds the file.

| Moment | Replay course (each desktop course) | Discovery course |
| --- | --- | --- |
| The profile's shutdown complete (`ShutdownComplete`) | 16 held: `profile-runtime.lock` and the five stores with their `-wal` and `-shm` | not reached: the course never shut down |
| The desktop destroyed (`desktop.reset()`) | the same 16 held | 20 held: the same, plus `diagnostics.log`, `.git-process-boundary.lock` and a Git lock |
| After delivering deferred deletions (`sendPostedEvents(DeferredDelete)`) | 0 held after 2 ms | 17 held; 0 after 166 ms more of event processing |

So the profile's stores and its runtime lock close only when the desktop's
runtimes are deleted from the event loop. Both courses let `QTemporaryDir`
remove the root straight after the desktop goes, with no event processed in
between. POSIX unlinks an open file, so on macOS and Linux the root goes.
Windows refuses to delete an open file, `QDir::removeRecursively` stops short,
and `QTemporaryDir`'s destructor ignores the failure. The discovery course
also never shut its profile down, so its Git work was still in flight.

**The fix, in the courses.** `tests/end_to_end/private_data_root.hpp` gains
`remove_course_root`. It delivers deferred deletions and posted events and
retries the removal until the directory is gone, bounded at 10 s. If it is
not gone, it prints `COURSE_ROOT_NOT_REMOVED:<path>` and one
`COURSE_ROOT_LEFT:` line per entry left, and returns false.

- `application_attended_recent_replay`: each desktop course destroys its
  desktop after `ShutdownComplete` and requires its root removed; the two
  wire-fixture contracts require theirs removed too.
- `application_personal_discovery_public_path`: the course now shuts the
  profile down and waits for `ShutdownComplete`, as the replay course does,
  destroys the desktop and requires its root removed, and only then prints
  its pass line. The row passes on that line alone
  (`PASS_REGULAR_EXPRESSION`), so a root that stays fails it.

No product code changes.

**Nothing is left (AC2).**

| Row | Runs | Result | Left in `TEMP` |
| --- | --- | --- | --- |
| `application_attended_recent_replay` | 3 | 3 of 3 pass: 26.57, 25.65, 26.02 s | 0 entries each |
| `application_personal_discovery_public_path` | 3 | 3 of 3 pass: 10.31, 10.29, 10.29 s | 0 entries each |

`clang-format` is clean on the three files, and `clang-tidy` passes on both
courses with warnings as errors. The full `build/dev` tree builds with the
shared header changed.

**Negative controls (AC3).**

| Control | Result |
| --- | --- |
| `remove_course_root` with no event delivery and a zero bound: one removal attempt straight after the desktop goes | Both rows fail. Replay: `COURSE_ROOT_NOT_REMOVED:...\attended_recent_replay-GzBcpO after 16 ms`, 38 `COURSE_ROOT_LEFT` lines, `FAILED: the attended store's temporary root is removed`. Discovery: the same shape, 38 lines, `FAILED: the course's temporary root is removed`. |
| Both courses as they were at `84c2b3a3` | Both pass and leave their roots again: 2 `attended_recent_replay-*` (30 entries) and 1 `personal_discovery_public_path-*` (24 entries). |

**The 21 older leftovers.** All 21 matched the exact default-template names,
`attended_recent_replay-` or `personal_discovery_public_path-` and six
alphanumeric characters, which only these two executables produce, and each
held the course's own data root: `profile-runtime.lock` and `state/v1` stores,
at the top level in the eight made before [[BPR-1044]]'s `data/` layout and
under `data/` in the others. No process of either course was running. The
station removed them by that exact pattern after recording their contents.

**AC4.** On macOS and Linux the first removal attempt succeeds as before,
since an open file can be unlinked there, so the courses keep their path; the
discovery course's added shutdown is the one its replay sibling already makes
on every platform. No run has been made there from this station.

**Not fixed here.** A course that fails through `std::exit` still leaves its
root on every platform, because `std::exit` runs no destructor. That is the
failure path of every such course, not this card's.
