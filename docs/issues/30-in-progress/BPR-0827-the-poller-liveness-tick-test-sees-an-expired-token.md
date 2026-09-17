---
schema: "implement-rfc.card.v1"
id: "BPR-0827"
title: "The poller liveness tick test finds the liveness token already expired"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"dsp-002 BPR-0802 slice (pre-existing at HEAD)","found_at":"7fc0e498014c5266507fe909a468508053d962df","run":"run-20260916-stations","dispatch":"dsp-002"}
---

# BPR-0827: The poller liveness tick test finds the liveness token already expired

## Defect

`application_poller_liveness` fails on macOS. Its subtest
`test_a_tick_armed_before_destruction_reaches_nothing`
(`tests/end_to_end/github_application_liveness_tests.cpp`) reports "the
liveness token has expired by the time that tick runs". HEAD's own test
source failed the same way before [[BPR-0802]] changed that file, with the
marker set or unset (`build/r2-logs/dsp2-0802-baseline-head-liveness.log`).
After BPR-0802, the file's new boundary subtest passes first, and then the
tick subtest fails as before.

The last recorded pass is `build/logs/bpr0054/ctest-dev.log`, 2026-08-28
(main was near `70e5d8c5`; that is not a proven build revision). The test
file last changed in `47de7ae8` ([[BPR-0695]], 2026-09-08).

The test guards the BPR-0215/BPR-0217 crash class: a tick armed before
destruction must reach nothing. A red guard for that crash class is itself
a risk.

## Scope

- Find the first bad revision and the mechanism. Either the application now
  expires its token earlier, which may be a change to shutdown or liveness
  semantics, or the test's precondition drifted.
- Keep the assertion's meaning: a token born expired would make every
  production poller a no-op. Do not weaken it.

## Acceptance

- `application_poller_liveness` passes on macOS, and the cause is named here.
