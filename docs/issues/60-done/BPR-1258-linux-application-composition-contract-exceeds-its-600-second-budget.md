---
schema: "implement-rfc.card.v1"
id: "BPR-1258"
title: "Linux: application_composition_contract exceeds its 600 s timeout building its tests-disabled Release image"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-main-linux triage of the BPR-0905 Linux full suite, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1255--BPR-1258--f86522aa8587.json","related_cards":["BPR-0905","BPR-0974"]}
acceptance: [{"criterion":"The row passes on Linux within its budget, or the card is parked with the owner's options","outcome":"pass","revision":"b6d8a1247","evidence":{"kind":"command","detail":"ctest --preset dev -R '^application_composition_contract$' on bpr-main-linux (WSL2, 20 cores, GCC 15.2, build/dev) passed in 700.15 s against the new Linux TIMEOUT of 2160 s (3 x 700.4 s isolated, rounded up to a minute, per docs/development/build-and-test.md)."}},{"criterion":"The card records where the time goes","outcome":"pass","revision":"b6d8a1247","evidence":{"kind":"observation","detail":"700.4 s for all eight cases alone; 42.3 s for the seven source and graph cases; about 658 s for the nested tests-disabled Release configure, the --parallel 2 build of app_better_pull_review, and the image run. Also recorded: each killed run leaked about 1.4 GB into tmpfs /tmp."}}]
---

# BPR-1258: Linux: application_composition_contract exceeds its 600 s timeout building its tests-disabled Release image

Found by bpr-main-linux on 2026-10-01: WSL2 Linux 6.18, GCC 15.2, `build/dev`,
at `db021262a`. bpr-main-win's BPR-0905 full suite in this checkout reported
`***Timeout` at 600 s.

## Observation

Run alone, at load average about 1.9, the row times out at 600.08 s. It is not
contention.

`test_tests_off_release_graph_omits_public_path_fixture`
(`tests/end_to_end/composition_contract_tests.py`) configures a fresh
tests-disabled Release graph in a temporary directory. It builds
`app_better_pull_review` with `--parallel 2` and runs the image. On this
machine that build does not finish within the row's budget.

[[BPR-0974]] records Windows passing alone in 565 s, which is close to the
limit. CTestCostData records an average of 0 s here.

## Scope

- Measure the row's phases on Linux: configure, build and run.
- Apply the BPR-0905 sizing rule in `docs/development/build-and-test.md`. If the
  rule settles the budget or the design, do that. If it needs an owner choice,
  park it with the options.

## Acceptance

- The row passes on Linux within its budget, or the card is parked with the
  owner's options.
- The card records where the time goes.

## Where the time goes, and the bound (bpr-main-linux, 2026-10-01)

`ctest --timeout` does not override a row's own `TIMEOUT`, so the
measurement ran the row's exact CTest command directly, under the network
sandbox and with a 3,600 s ceiling. WSL2 had 20 cores, GCC 15.2 and `build/dev`.
The load average was about 2, which was the row's own `-j2` build.

| Phase | Time |
|---|---|
| All eight cases, alone | **700.4 s**, OK |
| The seven source and graph cases (`-k` selection) | 42.3 s |
| So `test_tests_off_release_graph_omits_public_path_fixture`: configure a tests-disabled Release graph, build `app_better_pull_review` with `--parallel 2`, run the image | about 658 s |

**The rule settles it.** `docs/development/build-and-test.md` ("Run budgets and
row timeouts") says that without a matching budget class, the bound is three
times an isolated measurement, rounded upward. A timeout is only a lower bound.
3 × 700.4 s = 2,101 s, which rounds up to a whole minute as **2,160 s**. That is
the same rounding the survey used for `sqlite_epoch_catalog_behavior_contract`.
Windows already has its own 1,800 s bound (823 s measured).

**Change (`b6d8a1247`).** In `tests/end_to_end/CMakeLists.txt`, `elseif(UNIX AND NOT
APPLE)` sets the row's `TIMEOUT` to 2160, with the measurement and multiplier
in a comment beside it. macOS keeps 600 s. The 2026-10-01 Mac full suite
recorded 363.41 s contended, a 1.65x margin. That is a known gap and is not
this card's.

| Check | Result |
|---|---|
| `ctest --preset dev -R '^application_composition_contract$'` after the change | **Passed, 700.15 s**, load 0.9 to 1.9 |
| `TIMEOUT` property after reconfigure | 2160 |
| `test_network_isolation_structure_contract` and `test_network_isolation_manifest` | Pass |

**A side effect worth knowing.** Each run that CTest killed at 600 s left its
nested Release tree, about 1.4 GB, under `/tmp/bpr*/release`. On this machine
`/tmp` is a RAM-backed tmpfs. Five of them (7.1 GB) were removed by hand. The
row's `contained_temporary_directory` cleans up only on a normal exit. A row
that passes no longer leaves them.

The design alternative, building the nested graph with more than two jobs, was
not taken. The rule settles the bound, and the parallelism is a choice about
the row's cost on a shared host.

Published: `git pull --rebase` renamed the change commit. `b6d8a1247`, named above and in the acceptance records, is `f91b477f6` on main.
