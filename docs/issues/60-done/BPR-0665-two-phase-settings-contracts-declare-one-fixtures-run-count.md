---
schema: "implement-rfc.card.v1"
id: "BPR-0665"
title: "Two-phase settings contracts declare one fixture's run count"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P3"
acceptance: [{"criterion":"Every automated-run bound declares actual budgeted process count and configures","outcome":"pass","revision":"d370a7fac6e7b26bbf85a96c4e252a582e294541 plus tests/end_to_end/CMakeLists.txt in finish","evidence":{"kind":"observation","detail":"Audited all 43 bounds against direct invocations/scripts. Corrected queue 2->4, native notification 3->4, workbench 1->2, defect 2->4, overview 2->4, behavior 2->4. Other bounds cover their full-budget runs; native appearance startup/parser checks reside in reserve; appearance theme includes POSIX third run conservatively. cmake --preset dev passed; three-quarter bounds admitted."}},{"criterion":"Focused direct contracts and documentation","outcome":"pass","revision":"d370a7fac6e7b26bbf85a96c4e252a582e294541 plus finish","evidence":{"kind":"command","detail":"ctest --preset dev -R ^application_(queue_composition_contract|workbench_composition_contract|defect_composition_contract|overview_since_review_contract|review_behaviour_contract)$ --output-on-failure -j 1: 5/5 passed, 283.73s. Native notification attended course not rerun: only registration timeout/count changes; no native delivery behavior claim. Documentation gate and git diff --check pass."}}]
---

# Two-phase settings contracts declare one fixture's run count

`bpr_bound_automated_run` exists so a stalled stage is killed by the
application's own budget with a stage, verdict and evidence, rather than
by CTest with none. It checks that the runs a test permits itself fit in
three quarters of the CTest timeout.
[BPR-0654](BPR-0654-resolve-the-files-changed-since-my-last-review.md)'s
`application_overview_since_review_contract` and
[BPR-0661](BPR-0661-review-behaviour-preferences.md)'s
`application_review_behaviour_contract` each run two phases over two
fixtures, four application processes, but declare `RUNS 2` inside a 120 s
timeout. Four 40 000 ms budgets do not fit: on a host slow enough for the
budgets to be reached, CTest kills the test with no verdict, which is the
outcome the bound was written to prevent.
[BPR-0660](BPR-0660-enterprise-servers-and-session-facts-on-accounts-and-hosts.md)'s
`application_saved_server_contract` declares `RUNS 4` and takes a 240 s
timeout for the same shape.

## Scope

- Declare the actual process count for every two-phase contract that
  loops over fixtures, and widen the CTest timeout to match.
- Check the other `bpr_bound_automated_run` callers for the same
  understatement rather than only these two.
- Owned paths: `tests/end_to_end/CMakeLists.txt`.

## Acceptance

- Every automated-run bound names the number of application processes its
  script starts, and configuration passes with those numbers.

## Implementation anchor

RFC_BASE_SHA: `9a175e81` (review-resume baseline, 2026-09-07).
Routing: standard | Reason: coupled CMake run-count audit retained by integrator.
Owned path: `tests/end_to_end/CMakeLists.txt`. Audit each bound against its
selected script, correct process counts and corresponding timeout, then configure
the dev graph and run the affected direct contracts. No product behavior change.
