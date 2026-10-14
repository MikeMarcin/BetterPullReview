---
schema: "implement-rfc.card.v1"
id: "BPR-1496"
title: "The gh CLI Git smart transport test runs at its timeout"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1492--BPR-1496--90f0a51696a5.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) on main cd4860e30, reproduced alone on a quiet machine, owner release request 2026-10-13"}
---

# The gh CLI Git smart transport test runs at its timeout

`gh_cli_git_smart_transport` timed out (60 s) in the full gate on main at
cd4860e30. Alone on a quiet machine it passed in 55.4 s, against 36.9 s in
the 2026-10-12 gate. The row's `TIMEOUT 60` leaves no margin, so the test
fails under the gate's load.

## Acceptance

- AC1: What the test spends its time on is measured, and the reason it grew
  from about 37 s to about 55 s is named.
- AC2: The test finishes well inside its timeout on a loaded machine: by
  removing needless waiting, or, if the work is genuinely that long, by a
  justified timeout with the measurement beside it.

## Boundaries

No test course is removed.

## Evidence

Measured on 2026-10-13 on the owner's Mac (macOS, arm64), with other agents
building; each run's load averages are from `uptime` just before it.

AC1, where the time goes. Per-section marks and per-kind launch counters
(temporary instrumentation, not committed) on main a8fd17aea:

- The course launches 969 sandboxed fake `gh` processes. Each costs about
  35 ms, almost all child start-up: `sandbox-exec` alone takes about 14 ms
  and the 23 MB test executable's start-up about 22 ms. Starting the fake
  before `QCoreApplication` saved nothing measurable.
- 84 of those are exact credential acquisitions, six launches each
  (`gh --version`, four `--help` topics, `auth token`): 504 launches, about
  half the total. 98ba1d249 (BPR-1425/1426, 2026-10-13) added one to every
  blob read, batch read and pack acquisition, after the qualification probes
  those operations already run. The course's own launch-count checks
  (seven processes per text read, fourteen for a cold open) pin this
  behaviour, so the launches are the product's work, not the test's.
- Two product deadlines are waited out in full, 10 s each: Git's
  `clone -h` capability probe and gh's first help topic. Both cases date
  from 2026-10-07, so they were already in the 36.9 s run.

The cause of the growth from about 37 s to about 55 s is therefore 98ba1d249's
per-operation credential acquisition, which roughly doubled the gh launches
(about 18 s at 35 ms each). Neither 051a17367 (shared process supervision,
which waits on notifications) nor 170c706c0 added waiting.

AC2. The two held-probe cases (Git capability cancel and timeout; gh help
cancel, timeout and the repaired retry through the same authority) now run in
`probe_deadline_course` on a fixture root of their own, on a thread beside the
main course, with the same transports, files and checks. A held probe blocks
every launch of its kind in its root, which is why it needs its own root. No
case or check is removed. The main course keeps a retry after its other refused
help probes. The remaining about 35 s is genuine launch work, so the row's
timeout is 90 s, with the measurement in a comment beside it.

| Binary | Run | Load before | Wall time |
|---|---|---|---|
| before (a8fd17aea) | direct | 96.48 | 94.9 s |
| before | direct | 21.76 | 67.5 s |
| after | direct | 21.06 | 38.5 s |
| before | direct | 16.28 | 57.7 s |
| after | direct | 14.30 | 35.5 s |
| after | direct | 24.59 | 53.1 s |
| after | direct | 25.66 | 54.8 s |
| after | `ctest`, TIMEOUT 90 | 23.03 | 52.4 s |

The before and after rows from the 21.76 load onwards were run one after the
other in that order. Load moves within seconds on this machine, so only the
paired rows compare directly: the change saves about 22 to 29 s.
`test_network_isolation_manifest` passes (1.7 s). An `lsof -i` poll every
0.5 s through a whole run found no socket held by the test, `gh`, `git` or
`sandbox-exec`.
