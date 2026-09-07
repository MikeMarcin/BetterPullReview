---
schema: "implement-rfc.card.v1"
id: "BPR-0665"
title: "Two-phase settings contracts declare one fixture's run count"
status: "Backlog"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P3"
---

# Two-phase settings contracts declare one fixture's run count

`bpr_bound_automated_run` exists so a stalled stage is killed by the
application's own budget with a stage, verdict and evidence, rather than
by CTest with none. It checks that the runs a test permits itself fit in
three quarters of the CTest timeout.
[BPR-0654](../60-done/BPR-0654-overview-changed-since-my-last-review.md)'s
`application_overview_since_review_contract` and
[BPR-0661](../60-done/BPR-0661-review-behaviour-preferences.md)'s
`application_review_behaviour_contract` each run two phases over two
fixtures, four application processes, but declare `RUNS 2` inside a 120 s
timeout. Four 40 000 ms budgets do not fit: on a host slow enough for the
budgets to be reached, CTest kills the test with no verdict, which is the
outcome the bound was written to prevent.
[BPR-0660](../60-done/BPR-0660-enterprise-servers-and-session-facts-on-accounts-and-hosts.md)'s
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
