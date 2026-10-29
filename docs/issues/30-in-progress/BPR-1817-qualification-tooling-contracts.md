---
schema: "implement-rfc.card.v1"
id: "BPR-1817"
title: "Keep release tooling tests within their declared contracts"
status: "In Progress"
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

## Isolated correction and initial evidence

The three repaired contracts pass on macOS: nine skip-report tests, six contained
temporary-usage tests and nine registry tests. Both affected mocked publication
paths pass. The release-build script reports 16 passes and one environment
refusal: its unchanged cold/warm production observation requires an offline CPM
cache absent from this isolated checkout. That raw failure is retained; no full
17-case passing result is claimed. Native successor checks remain BPR-1811 work.

The temporary-directory and skip-report defects already exist in published
0.4.0 producer `0941326aee0f51002e205784a1b220a01ecba384`. The prior six full
reports were taken at older source `144de719a8d972162f92ec04f98ba11293ae0a8f`,
so they do not establish execution of the final producer's added tooling. The
workbench unit test and registry are unchanged from the published producer, but
the actual C++ input changed afterward: its original seal correctly refuses
current source. Positive declaration tests now copy current inputs into private
fixtures; they do not renew the production seal.

Negative controls reject changed source and all seven transitive inputs, as well
as incorrect selectors, output roots, scales, unset variables, overrides and
manifest occurrence credit. Production registries, inventory, validators and
scheduling CMake remain byte-for-byte unchanged. External evidence is in
`qualification-contract-repairs-r1` under the recorded run root. Frozen build-4
raw failures and its independently running native suites remain unchanged.
