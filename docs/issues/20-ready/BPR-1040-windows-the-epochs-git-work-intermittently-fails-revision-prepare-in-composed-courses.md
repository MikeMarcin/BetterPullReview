---
schema: "implement-rfc.card.v1"
id: "BPR-1040"
title: "Windows: the epoch's Git subprocess intermittently fails revision-prepare in composed courses"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win rows under BPR-1020 and BPR-1030","found_at":"38cc26d7","reproduced_at":["38cc26d7","4fd95fe5","04d22b4a"],"related_cards":["BPR-1030","BPR-1038","BPR-0441"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1038--BPR-1042--5b0f0a679128.json"}
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
