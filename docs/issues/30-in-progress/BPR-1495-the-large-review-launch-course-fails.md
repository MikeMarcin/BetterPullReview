---
schema: "implement-rfc.card.v1"
id: "BPR-1495"
title: "The large review launch course fails"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1492--BPR-1496--90f0a51696a5.json","origin":"0.2.0 release normal gate (scripts/run_test_suite.py --preset dev) on main cd4860e30, reproduced alone on a quiet machine, owner release request 2026-10-13"}
---

# The large review launch course fails

`application_large_review_launch` fails on main at cd4860e30 in the full gate
and again alone, after `LARGE_REVIEW_RESPONSIVENESS_MEASURED`, without a
FAILED line naming which check failed. It also failed in the 2026-10-12 gate
(2.9 s). `application_large_review_responsiveness` and
`application_large_review_history` pass alone.

## Acceptance

- AC1: The failing check is named, with its cause and the commit that
  introduced it. A budget miss is reported as a miss; no budget or assertion
  is loosened to pass.
- AC2: If the course does not name a failing check, it is changed to name it.
- AC3: `application_large_review_launch` passes on macOS, or the remaining
  miss is reported with its measurement for the owner.

## Boundaries

No change to the course's budgets.

## Evidence

Measured on 2026-10-13 in a worktree at a8fd17aea (dev preset, Debug), while
other agents were building: load averages 21 to 41 throughout. Every timing
below is load-contaminated, so it compares builds run alternately under the
same load. It is not a quiet-machine figure.

- AC1, failing check: the `ProcessToMainMarkerMs` limit of 100 ms in
  `tests/end_to_end/large_review_launch.py`. The cd4860e30 gate's own report
  (`.claude/worktrees/release-gate/build/dev/tests/end_to_end/large-review-launch.json`)
  records `ProcessToMainMarkerMs` 129.8 against the 100 ms limit, with
  `ProcessToHomeMarkerMs` 933.6, exit code 0 and `WithinLimits: false`. The
  fixture printed its success marker, and the script then returned 1 without
  saying why. The earlier 2.9 s failure in the 2026-10-12 gate was a different
  failure that names itself
  (`large-review-benchmark-failed: selected provider/Git path or replay exhaustion`).
- Cause: almost all of the pre-main time is dyld coalescing the weak
  definitions that the Debug fixture executable exports. A `DYLD_PRINT_INITIALIZERS`
  trace shows about 126 ms passing before the first initializer, in libSystem.
  The fixture's own 12 static initializers together take under 1 ms. A
  `main` that links the same Qt frameworks reaches the marker in 13 to 18 ms
  (median). `dyld_info -fixups` counts `<weak-def-coalesce>` binds at 26,166 in
  the 2026-10-12 14:36 Debug build and 30,190 at a8fd17aea. Of the 4,024 added,
  about 2,690 are `absl::`/`re2::` and about 630 are `fmt::`. The Release
  `BetterPullReview` executable (2026-09-25 build) has 118, so the shipped app
  does not pay this cost: it is a Debug-fixture cost, not a product launch
  regression.
- Introducing commits: there is no single one. The margin has worn away as the
  composition grew. At af7144da1 (v0.1.10) the row measured 82.7 ms on a quiet
  machine and already failed once under gate load. 32ccfd8ad (BPR-1431, RE2
  and Abseil linked through the regex adapter) is the largest single step,
  and 8172a3a63 (BPR-1439, fmt) is the second.
- Measurements: three alternating rounds of 15 launches each, timed to
  `LARGE_REVIEW_MAIN_ENTERED` at load averages of about 35. Medians: af7144da1
  99.9, 97.9 and 96.8 ms; the 2026-10-12 14:36 build 101.8, 100.0 and 98.8 ms;
  a8fd17aea 116.9, 112.4 and 114.0 ms. Three CTest runs at a8fd17aea
  before the script change (load averages 37, 31 and 28) measured
  `ProcessToMainMarkerMs` at 2313.5, 115.1 and 112.0. Three after it (load
  averages 26, 27 and 25) measured 137.5, 134.5 and 188.6, and each run named
  its refusal.
- Budget miss: `ProcessToMainMarkerMs` misses its 100 ms limit at a8fd17aea under
  the load measured here, and it measured 129.8 ms in the cd4860e30 gate. No
  quiet-machine figure was available. Scaling af7144da1's quiet 82.7 ms by
  the step measured under load (about 15 ms) puts a8fd17aea near 95 to 100 ms
  quiet, so the row has no margin even on a quiet machine. No budget or
  assertion was changed.
- Candidate remedy, for the owner and not applied: relinking the same object
  files with an empty exported-symbols list
  (`-Wl,-exported_symbols_list,<empty file>`, an experiment build kept outside
  the tree) cuts the weak-coalesce binds from 30,190 to 39. Main-marker medians
  fell from 129 to 131 ms to 22 to 25 ms over three rounds at load averages of
  about 24, and in paired course runs from 110 to 130 ms to 19 to 25 ms. One of
  the experiment binary's three course runs printed
  `LARGE_REVIEW_LAUNCH_OBSERVED`. In the other two, the fixture's own
  responsiveness budget refused under load, and the unchanged binary did the
  same in one of its three runs. Whether the fixture executable should export
  no symbols, which changes what the row measures, is the owner's decision.
- AC2: `large_review_launch.py` now prints one
  `LARGE_REVIEW_LAUNCH_FAILED: <check>` line for each failed check (the
  fixture's exit code, a missing marker, marker order, and each enforced
  limit with its measurement). The pass and fail conditions are unchanged.
- AC3: not met. The miss above remains and is reported for the owner.
