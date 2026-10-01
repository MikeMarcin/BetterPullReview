---
schema: "implement-rfc.card.v1"
id: "BPR-1258"
title: "Linux: application_composition_contract exceeds its 600 s timeout building its tests-disabled Release image"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"found_by":"bpr-main-linux triage of the BPR-0905 Linux full suite, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1255--BPR-1258--f86522aa8587.json","related_cards":["BPR-0905","BPR-0974"]}
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
