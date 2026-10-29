---
schema: "implement-rfc.card.v1"
id: "BPR-1817"
title: "Keep release tooling tests within their declared contracts"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "release-v041-same-preview-waivers-r1", "external_state": "/Users/jaeger/.codex/release-runs/v0.4.1-20261029", "milestone_plan": {"id": "release-v041", "path": "docs/rfcs/plans/release-v041/milestone-plan.json"}}
---

# Keep release tooling tests within their declared contracts

The frozen build-4 Mac Release suite reports three tooling-contract failures:
`skip_report_contract`, `contained_temporary_usage_contract` and
`command_correctness_registry_contract`. Their raw results remain failures.
This isolated repair does not modify the active frozen source or renew any
production correctness-scheduling seal. It is engineering follow-up, with no
Debug timing waiver or passing credit inferred from historical exceptions.

## Acceptance

- AC1: Preserve the observed failures and compare their precise causes with
  the previous release. Use the shared contained temporary-root and skip-report
  support in the affected release tests. Exercise registry declaration semantics
  on private current fixtures while retaining stale-source/input refusal,
  command/environment mutation controls and the production scheduling boundary.
- AC2: Run the three focused contracts and affected lightweight test paths on
  the exact correction; retain raw results, native-only skips and source identity.
  Keep frozen build-4 suites and producer artifacts unchanged. Record the exact
  test-only source bridge and require a successor cohort before promoting any
  changed source as a producer.

## Execution boundary

Own only the isolated `qualification-contracts-r1` checkout linked to the private
release controller. The active Mac and PC build-4 suites continue independently.
Use small source/script operations and bounded focused checks while the Mac suite
holds its fixed capacity reserve. No heavy build, package transfer, additional
review gate, production seal refresh or unrelated source change is authorized.
