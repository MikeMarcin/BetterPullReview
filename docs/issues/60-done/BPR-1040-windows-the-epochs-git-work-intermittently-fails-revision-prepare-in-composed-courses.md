---
schema: "implement-rfc.card.v1"
id: "BPR-1040"
title: "Windows: the epoch's Git subprocess intermittently fails revision-prepare in composed courses"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win rows under BPR-1020 and BPR-1030","found_at":"38cc26d7","reproduced_at":["38cc26d7","4fd95fe5","04d22b4a"],"related_cards":["BPR-1030","BPR-1038","BPR-0441"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1038--BPR-1042--5b0f0a679128.json"}
acceptance: [{"criterion":"AC1 The failing operation and its Windows error are named with evidence from at least one reproduced failure","outcome":"pass","revision":"8595684c","evidence":{"kind":"command","detail":"bpr-main-win, build/dev MSVC 19.51 Debug: with every data root's diagnostics kept and reverted traces in the Git cache, the review-restoration course (about 18 of 19 runs failing at the epoch) named three operations on a just-written pack spool: the publish rename (op=epoch epoch_stage=protocol_acquire failure=pack_sink pack_failure=storage, 3 of 5 kept runs); the rename of the completed spool for removal (BPR1040_RENAME rename_no_replace error=32, ERROR_SHARING_VIOLATION on incoming/v1/<hash>.pack.part, Restart Manager count=0, the same rename succeeding 50 ms later, EPOCH_CACHE_FAILURE:7); and the exclusive open that feeds the spool to Git (revision-prepare:6 after cache 10).","path":"docs/issues/60-done/BPR-1040-windows-the-epochs-git-work-intermittently-fails-revision-prepare-in-composed-courses.md#named-a-just-written-spool-is-briefly-held-a-bounded-retry-lands-bpr-main-win-2026-09-23"}},{"criterion":"AC2 application_release_maintenance_public_path reaches its purge step without an epoch failure in N of N runs (N >= 10) on Windows, and so do application_attended_recent_replay and application_profile_multi_window_public_path (N >= 5 each)","outcome":"pass","revision":"8595684c","evidence":{"kind":"command","detail":"build/windows-dev at 8595684c (the fix as committed, no trace), serial ctest per run: release maintenance 10 of 10 reach the purge step (stopping at BPR-1038's inventory refusal, since fixed in a37f7ab6, after which the course passes whole) with 0 EPOCH_CACHE_FAILURE/EPOCH_EFFECT_FAILED lines; attended_recent_replay 6 of 6 and profile_multi_window 6 of 6 pass with 0 epoch lines (5 each in build/logs/bpr-1040-main-win/ac2, 1 each in BPR-1044's 13-row run).","path":"docs/issues/60-done/BPR-1040-windows-the-epochs-git-work-intermittently-fails-revision-prepare-in-composed-courses.md#ac2-met-on-the-committed-fix-bpr-main-win-2026-09-23"}},{"criterion":"AC3 macOS and Linux keep their paths, or any shared change is run there","outcome":"pass","revision":"8595684c","evidence":{"kind":"freeform","detail":"Every change is Windows-only: windows_sharing_retry.hpp is inside Q_OS_WIN, and its three call sites are the Q_OS_WIN branches of publish_spool/rename_no_replace (move_file_no_replace) in installed_git_cache.cpp and of private_input_file::open in installed_git_process.cpp. The POSIX branches are byte for byte unchanged."}}]
---

# BPR-1040: Windows: the epoch's Git subprocess intermittently fails revision-prepare in composed courses

## Defect

On Windows, composed courses that build an epoch through installed Git
sometimes fail at it. The epoch host prints
`EPOCH_EFFECT_FAILED:revision-prepare:<failure>`
(`src/application/src/epoch_application_host.cpp:3307`), and the course then
stops at its next epoch check. For example:

- "real Git producer publishes an exact epoch";
- "exact window comparison epoch becomes ready";
- "UIforETW retained comparison ready".

The same course passes on the next run. The adapter's code is not the same
from one failure to the next. When the cache reported it, the line before
was `EPOCH_CACHE_FAILURE:<code>:<detail>` (`:361`), with an empty detail each
time:

| `EPOCH_EFFECT_FAILED` | `EPOCH_CACHE_FAILURE` before it | Meaning |
| --- | --- | --- |
| `revision-prepare:6` (`process_failure`) | `10` (`git_cache_failure::process_failure`) | Installed Git exited unsuccessfully |
| `revision-prepare:0` (`unavailable`) | `7` (`spool_failure`) | The cache could not write its spool |
| `revision-prepare:0` | none | The failure came from a path that prints no adapter code |

Only Windows has shown it. Every row involved is registered uncontained on
Windows. The files a Git process and the application both touch, and the
sharing rules Windows applies to them, are the obvious common factor. That is
a hypothesis, not a finding.

## Evidence (bpr-main-win, MSVC 19.51 Debug, Qt 6.11.2, offscreen, serial)

The logs are station records under `build/logs/` and are not committed.

| Revision | Course | Failed at the epoch | Codes |
| --- | --- | --- | --- |
| `38cc26d7` | `application_attended_recent_replay` | 1 run (`bpr-1020-main-win/uncontained-1.log`) | `revision-prepare:0`, no cache line |
| `7d68d5a3`…`4fd95fe5` | `application_release_maintenance_public_path` | 2 of 9 ([[BPR-1030]]) | `:0` no cache line; `:6` after cache `10` |
| `7d68d5a3`…`4fd95fe5` | `application_attended_recent_replay` | 1 of 8 | `:6` after cache `10` |
| `4fd95fe5` | `application_profile_multi_window_public_path` | 1 of 3 | `:6` after cache `10` |
| `04d22b4a` | `application_release_maintenance_public_path` | **3 of 7** (`bpr-1038-1041-main-win/rows.log`, `maint-{1..6}.log`) | `:0` after cache `7` (twice); `:0` no cache line (once) |

`application_logging` prints `EPOCH_CACHE_FAILURE:2:` and
`revision-prepare:0` in every run and passes. That course provokes the
failure on purpose, so it is not counted here.

## Scope

- Name the failure. The adapter keeps Git's exit status and message class,
  and the core deliberately does not ([[BPR-0441]]). Under the testing build,
  print the failing Git command's verb, exit code and the adapter's
  classification when the cache reports `process_failure` or
  `spool_failure`. Also print the path that reaches `revision-prepare:0`
  with no cache line.
- With the command and its error in hand, find the cause. For example: a
  sharing violation on a file the application or another Git process holds,
  a lock file left behind by a Git process that was cancelled, or a spool
  write refused because the file is still open.
- Fix it in the layer that owns it. If Windows needs a bounded retry, it must
  be named and bounded, not a loop that hides the cause.

## Acceptance

- AC1 The failing operation and its Windows error are named with evidence
  from at least one reproduced failure.
- AC2 `application_release_maintenance_public_path` reaches its purge step
  (or passes, once [[BPR-1038]] is fixed) without an epoch failure in N of N
  runs (N ≥ 10) on Windows, and so do
  `application_attended_recent_replay` and
  `application_profile_multi_window_public_path` (N ≥ 5 each).
- AC3 macOS and Linux keep their paths, or any shared change is run there.

## Named: a just-written spool is briefly held; a bounded retry lands (bpr-main-win, 2026-09-23)

Run `bpr-main-win-20260923`, `build/dev` (MSVC 19.51 Debug, Qt 6.11.2),
serial and offscreen, at `585d8846` plus this change. Microsoft Defender
real-time protection is on, as it is by default. The logs are station records
under `build/logs/bpr-1040-main-win/` and are not committed.

**The course that reproduces it.** With [[BPR-0876]]'s copied roots fixed,
`application_profile_window_review_restoration_public_path`, run uncontained,
failed at the epoch in about 18 of 19 runs. A station wrapper kept every data
root's `diagnostics.log`. Reverted traces then named the failing operation. In
each case it was a pack spool file that the adapter had written moments
before (AC1):

| Where | What the diagnostics or the trace showed |
| --- | --- |
| Publishing the received spool: `.pack.receiving` renamed to `.pack.part` (`publish_spool`) | `op=epoch outcome=failed epoch_stage=protocol_acquire failure=pack_sink pack_failure=storage`, 3 of 5 kept runs |
| Removing the completed spool after staging: `remove_exact_private_file` renames it to a quarantine name first | `BPR1040_RENAME rename_no_replace error=32` ("being used by another process") on `incoming\v1\<hash>.pack.part`. The Restart Manager listed **no** holder (`BPR1040_HOLDERS count=0`), and the same rename **succeeded 50 ms later**. The epoch reported `EPOCH_CACHE_FAILURE:7` (`spool_failure`) |
| Opening the completed spool exclusively to feed it to Git as standard input (`private_input_file::open`, `share 0`) | Git's run failed before it started, as the card's `revision-prepare:6` after cache `10`. Same shape: a sharing refusal on a file just written. The trace log for this one was not kept |

A holder that the Restart Manager cannot name, that lets go within tens of
milliseconds, and that appears only on files just written, looks like a
real-time scanner reading each new file. No product process held these files.

**The fix, in the layer that owns it.** `windows_sharing_retry.hpp` adds
`detail::retry_while_held`. It repeats one Windows file operation while the
error is `ERROR_SHARING_VIOLATION`, `ERROR_LOCK_VIOLATION` or
`ERROR_ACCESS_DENIED`. The retry is named and bounded: eight waits from 5 ms,
doubling, about 1.3 s in all. Then it returns the last error, so the caller
fails exactly as before. Three call sites use it: the no-replace move behind
`publish_spool` and `rename_no_replace` (`move_file_no_replace` in
`installed_git_cache.cpp`), and the exclusive open in `private_input_file`
(`installed_git_process.cpp`). All of it is inside `Q_OS_WIN`.

**Measured after the fix,** same course, same wrapper:

| Runs | Passed | Stopped at the epoch | Stopped elsewhere |
| --- | --- | --- | --- |
| 8 at the candidate | 6 | 1: `EPOCH_CACHE_FAILURE:7`, `revision_prepare failure=spool_failure`, before the trace below was in | 1: "durable record equals actual accepted public reading", no epoch line, under heavy concurrent load from other rows |

Against about 18 of 19 before, that is the named cause gone. One
`spool_failure` in eight remains unexplained. The build now running the AC2
courses carries a reverted trace on every `spool_failure` return and on each
step of `remove_exact_private_file`, so its next occurrence names its site.
The "durable record" stop is not an epoch failure, and it is recorded here
only so that it is measured again.

### Still open

- **AC2.** `application_release_maintenance_public_path` must reach its purge
  step without an epoch failure in N of N runs (N ≥ 10), and
  `application_attended_recent_replay` and
  `application_profile_multi_window_public_path` must do the same (N ≥ 5
  each). Those runs are next on this station.
- **AC3.** Every change is inside `Q_OS_WIN`, so macOS and Linux keep their
  paths.
- Scope's permanent testing-build diagnostic is not added. The traces above
  named the failing operation. A permanent line would need a decision about
  what the adapter may print, and [[BPR-0441]] keeps Git's text out of the
  core.

## AC2 met on the committed fix (bpr-main-win, 2026-09-23)

The runs used `build/windows-dev` (Visual Studio generator, MSVC 19.51, Debug)
built at `8595684c`, the fix as committed, with no trace in it. Each run was
its own serial `ctest --preset windows-dev -j1 -V` call. The logs are station
records under `build/logs/bpr-1040-main-win/ac2/` and
`build/logs/bpr-1044-main-win/rows13.log`, and are not committed.

| Course | Runs | Result | Epoch lines |
| --- | --- | --- | --- |
| `application_release_maintenance_public_path` | 10 | 10 of 10 reach the purge step and stop there, at "native full-profile inventory returns", which is [[BPR-1038]]'s refusal | 0 `EPOCH_CACHE_FAILURE` or `EPOCH_EFFECT_FAILED` |
| `application_attended_recent_replay` | 6 (5 here, 1 in BPR-1044's run) | 6 of 6 pass | 0 |
| `application_profile_multi_window_public_path` | 6 (5 here, 1 in BPR-1044's run) | 6 of 6 pass | 0 |

Before the fix, the release maintenance course failed at the epoch in 3 of 7
runs at `04d22b4a`. The replay course failed there in 1 of 8, and the
multi-window course in 1 of 3. With the fix, none of 22 runs did.

[[BPR-1038]] has since been fixed as well, and on that candidate the release
maintenance course passes whole (14.33 s), with no epoch line.

**AC1** is met by the table in the section above: the three failing
operations and their Windows errors, from reproduced failures. **AC2** is met
here. **AC3**: every change is inside `Q_OS_WIN` in the Git adapter's Windows
branches, so macOS and Linux keep their paths. The one `spool_failure` seen in
eight review-restoration runs before the trace went in did not recur in the 6
traced runs or the 22 here. It remains unexplained and would name its site
under the trace that is kept at the station.

## Post-close pointer (2026-09-24)

For the exclusive-open shape, "No product process held these files" is
corrected by [[BPR-1104]]'s "Named twice, fixed at the launch and at the
delete": the holder was this process's own inherited-handle `git` child.
