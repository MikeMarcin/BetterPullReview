---
schema: "implement-rfc.card.v1"
id: "BPR-1499"
title: "The macOS composition contract timeout does not cover a cold nested build"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"gate-repair","reservation":"docs/issues/_reservations/BPR-1499--BPR-1499--d4b5e0980137.json","origin":"0.2.0 release normal gate on main cd4860e30, owner release request 2026-10-13","related_cards":["BPR-1386","BPR-1258"]}
---

# The macOS composition contract timeout does not cover a cold nested build

`application_composition_contract` timed out at 600 s in the 0.2.0 baseline
gate on cd4860e30 and again in a focused run beside other agents' builds. It
passed alone in 43 s when its persistent nested Release tree
(`build/dev/tests/end_to_end/composition-contract-build`, [[BPR-1386]]) was
warm. Its `tests_off_release_graph` case configures and builds
`app_better_pull_review` in Release at `--parallel 2`. After a large source
change, or in a checkout whose nested tree is stale, that build alone fills most
of the 600 s macOS bound. Windows (1800 s) and Linux (2160 s, [[BPR-1258]])
already carry measured bounds.

## Acceptance

- AC1: A cold nested build is measured on macOS with its load recorded, and
  the macOS timeout is sized from it by the run-budget rule in
  `docs/development/build-and-test.md`, with the measurement beside it.
- AC2: The row passes on macOS with the new bound.

## Boundaries

No case is removed and the nested build's parallelism is unchanged.
